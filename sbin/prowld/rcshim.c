/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * rc.d compatibility shim: parse REQUIRE/PROVIDE/BEFORE/KEYWORD headers
 * from rc.d scripts and wrap them as prowld jobs of type JOB_TYPE_RCSHIM.
 *
 * Also provides rcconf_is_enabled() for reading <name>_enable from rc.conf.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "prowld.h"

/* rc.d script header tokens */
#define RCD_PROVIDE	"# PROVIDE:"
#define RCD_PROVIDES	"# PROVIDES:"
#define RCD_REQUIRE	"# REQUIRE:"
#define RCD_REQUIRES	"# REQUIRES:"
#define RCD_BEFORE	"# BEFORE:"
#define RCD_KEYWORD	"# KEYWORD:"
#define RCD_KEYWORDS	"# KEYWORDS:"
#define RCD_RCVAR	"rcvar="

/*
 * Skip leading whitespace and return pointer into s.
 */
static const char *
skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/*
 * Extract whitespace-separated tokens from a string and add them to a
 * char[][] array.  Returns the new total count.
 */
static int
tokenize_into(const char *line, char dest[][PROWL_LABEL_MAX], int cur, int max)
{
	char buf[PROWL_LABEL_MAX];
	const char *p = line;
	int n = cur;

	while (*p != '\0' && n < max) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\0' || *p == '\n' || *p == '#')
			break;

		size_t i = 0;
		while (*p != '\0' && *p != ' ' && *p != '\t' &&
		    *p != '\n' && *p != '#' && i < sizeof(buf) - 1) {
			buf[i++] = *p++;
		}
		buf[i] = '\0';

		if (i > 0) {
			strlcpy(dest[n], buf, PROWL_LABEL_MAX);
			n++;
		}
	}
	return (n);
}

/*
 * Look for an "rcvar=" assignment at the start of a script line (p must
 * already be past leading whitespace).
 *
 * *found is set when the script names a non-empty rcvar.  An empty
 * "rcvar=" means the service has no enable switch and always runs, so it is
 * treated like a script with no rcvar at all.  name receives the variable
 * name when it is a plain literal (e.g. "syslogd_enable"); it is left empty
 * when the value needs shell expansion (e.g. "${name}_enable"), in which
 * case the caller falls back to <PROVIDE>_enable.  Only the first rcvar=
 * line in a script is used.
 */
static void
rcvar_scan(const char *p, bool *found, char *name, size_t namesz)
{
	const char *q;
	size_t n;

	if (*found || strncmp(p, RCD_RCVAR, strlen(RCD_RCVAR)) != 0)
		return;
	p += strlen(RCD_RCVAR);
	if (*p == '"' || *p == '\'')
		p++;
	if (*p == '\0' || *p == '\n' || *p == '"' || *p == '\'')
		return;		/* empty rcvar: no enable switch */

	*found = true;
	name[0] = '\0';
	for (q = p; *q != '\0' && *q != '"' && *q != '\'' && *q != '\n' &&
	    *q != ' ' && *q != '\t' && *q != ';'; q++) {
		if (!isalnum((unsigned char)*q) && *q != '_')
			return;	/* needs expansion: caller uses fallback */
	}
	n = (size_t)(q - p);
	if (n == 0 || n >= namesz)
		return;
	memcpy(name, p, n);
	name[n] = '\0';
}

/*
 * If the line is key="value" and out is still empty, copy the value: up to
 * a quote, a blank or a semicolon.  A value that does not fit is not kept.
 */
static void
assign_scan(const char *p, const char *key, char *out, size_t outsz)
{
	const char *q;
	size_t n;

	if (out[0] != '\0' || strncmp(p, key, strlen(key)) != 0 ||
	    p[strlen(key)] != '=')
		return;
	p += strlen(key) + 1;
	if (*p == '"' || *p == '\'')
		p++;
	for (q = p; *q != '\0' && *q != '"' && *q != '\'' && *q != '\n' &&
	    *q != ' ' && *q != '\t' && *q != ';'; q++)
		;
	n = (size_t)(q - p);
	if (n == 0 || n >= outsz)
		return;
	memcpy(out, p, n);
	out[n] = '\0';
}

/*
 * Turn the pidfile= value of an rc.d script into a path: $name and ${name}
 * are replaced by the script's name variable.  An empty result means the
 * pidfile is not a plain path.
 */
static void
pidfile_path(const char *raw, const char *name, char *out, size_t outsz)
{
	const char *p;
	size_t len = 0, nl = strlen(name);

	out[0] = '\0';
	if (raw[0] != '/')
		return;
	for (p = raw; *p != '\0'; ) {
		if (strncmp(p, "${name}", 7) == 0 && len + nl < outsz) {
			memcpy(out + len, name, nl);
			len += nl;
			p += 7;
		} else if (strncmp(p, "$name", 5) == 0 && len + nl < outsz) {
			memcpy(out + len, name, nl);
			len += nl;
			p += 5;
		} else if (*p == '$' || len + 1 >= outsz) {
			out[0] = '\0';
			return;
		} else {
			out[len++] = *p++;
		}
	}
	out[len] = '\0';
}

/*
 * Parse an rc.d script to extract its PROVIDE/REQUIRE/BEFORE/KEYWORD
 * headers and build a JOB_TYPE_RCSHIM job.
 *
 * Only the first PROVIDE token is used as the canonical rc_name; additional
 * tokens go into provides[].
 *
 * Returns the new job, or NULL on error or if a native unit already covers
 * this PROVIDE.
 */
static job_t *
rcshim_parse_script(int fd, const char *path, const char *scriptname)
{
	FILE *fp;
	char line[1024];
	job_t *job;
	char provides_raw[PROWL_PROVIDES_MAX][PROWL_LABEL_MAX];
	int provides_count = 0;
	char requires_raw[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int requires_count = 0;
	char before_raw[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int before_count = 0;
	bool keyword_shutdown = false;
	bool keyword_nojail = false;
	bool keyword_nostart = false;
	bool keyword_interactive = false;
	bool done_headers = false;
	bool has_rcvar = false;
	char rcvar_name[PROWL_LABEL_MAX];
	char name_var[PROWL_LABEL_MAX], pidfile_raw[PROWL_PATH_MAX];

	fp = fdopen(fd, "r");
	if (fp == NULL) {
		prowl_log(LOG_WARNING, "rcshim: fdopen %s: %m", path);
		close(fd);
		return (NULL);
	}

	/*
	 * rc.d headers appear before the first non-comment, non-blank line.
	 * Once we hit executable code the headers are done, but we keep
	 * reading to find the rcvar= line that names the enable variable.
	 */
	rcvar_name[0] = '\0';
	name_var[0] = '\0';
	pidfile_raw[0] = '\0';
	while (fgets(line, sizeof(line), fp) != NULL) {
		const char *p = line;

		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\0' || *p == '\n')
			continue;

		if (done_headers) {
			rcvar_scan(p, &has_rcvar, rcvar_name,
			    sizeof(rcvar_name));
			assign_scan(p, "name", name_var, sizeof(name_var));
			assign_scan(p, "pidfile", pidfile_raw,
			    sizeof(pidfile_raw));
			continue;
		}

		if (*p != '#') {
			done_headers = true;
			rcvar_scan(p, &has_rcvar, rcvar_name,
			    sizeof(rcvar_name));
			assign_scan(p, "name", name_var, sizeof(name_var));
			assign_scan(p, "pidfile", pidfile_raw,
			    sizeof(pidfile_raw));
			continue;
		}

		if (strncmp(p, RCD_PROVIDE, strlen(RCD_PROVIDE)) == 0)
			provides_count = tokenize_into(
			    p + strlen(RCD_PROVIDE),
			    provides_raw, provides_count, PROWL_PROVIDES_MAX);
		else if (strncmp(p, RCD_PROVIDES, strlen(RCD_PROVIDES)) == 0)
			provides_count = tokenize_into(
			    p + strlen(RCD_PROVIDES),
			    provides_raw, provides_count, PROWL_PROVIDES_MAX);
		else if (strncmp(p, RCD_REQUIRE, strlen(RCD_REQUIRE)) == 0)
			requires_count = tokenize_into(
			    p + strlen(RCD_REQUIRE),
			    requires_raw, requires_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_REQUIRES, strlen(RCD_REQUIRES)) == 0)
			requires_count = tokenize_into(
			    p + strlen(RCD_REQUIRES),
			    requires_raw, requires_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_BEFORE, strlen(RCD_BEFORE)) == 0)
			before_count = tokenize_into(
			    p + strlen(RCD_BEFORE),
			    before_raw, before_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_KEYWORD, strlen(RCD_KEYWORD)) == 0 ||
		    strncmp(p, RCD_KEYWORDS, strlen(RCD_KEYWORDS)) == 0) {
			const char *kw = (strncmp(p, RCD_KEYWORD,
			    strlen(RCD_KEYWORD)) == 0) ?
			    p + strlen(RCD_KEYWORD) :
			    p + strlen(RCD_KEYWORDS);
			if (strstr(kw, "shutdown") != NULL)
				keyword_shutdown = true;
			if (strstr(kw, "nojail") != NULL)
				keyword_nojail = true;
			if (strstr(kw, "nostart") != NULL)
				keyword_nostart = true;
			if (strstr(kw, "interactive") != NULL)
				keyword_interactive = true;
		}
	}
	fclose(fp);

	/*
	 * If no PROVIDE header found, use the script name as the provider.
	 */
	if (provides_count == 0) {
		strlcpy(provides_raw[0], scriptname, PROWL_LABEL_MAX);
		provides_count = 1;
	}

	/*
	 * Check whether a native unit already covers the primary PROVIDE.
	 * If so, skip this rc-shim (§7.3: native unit wins).
	 */
	for (int i = 0; i < provides_count; i++) {
		if (unit_provider_rejected(provides_raw[i])) {
			prowl_log(LOG_WARNING, "rcshim: rejected native provider "
			    "%s, skipping %s", provides_raw[i], path);
			return (NULL);
		}
	}
	/*
	 * Only a native unit displaces a script. Several scripts may share
	 * a PROVIDE (ccd, gbde and geli all provide "disks"); rcorder(8)
	 * runs each of them, so every one gets a shim.
	 */
	{
		job_t *other;

		TAILQ_FOREACH(other, &g_jobs, entries) {
			if (other->type != JOB_TYPE_RCSHIM &&
			    job_provides(other, provides_raw[0])) {
				prowl_log(LOG_DEBUG, "rcshim: native unit "
				    "covers %s, skipping %s", provides_raw[0],
				    path);
				return (NULL);
			}
		}
	}

	/*
	 * Never wrap our own rc.d script: running it would start a second
	 * prowld that takes over the control socket from this one.
	 */
	if (strcmp(scriptname, "prowld") == 0) {
		prowl_log(LOG_DEBUG, "rcshim: skipping %s (prowld itself)",
		    path);
		return (NULL);
	}

	/* Validate scriptname before embedding in label and paths */
	if (!label_valid(scriptname)) {
		prowl_log(LOG_WARNING,
		    "rcshim: invalid script name '%s', skipping", scriptname);
		return (NULL);
	}

	job = job_alloc();
	if (job == NULL)
		return (NULL);

	/* The script's pidfile tells which process its daemon is */
	{
		char pf[PROWL_PATH_MAX];

		pidfile_path(pidfile_raw, name_var[0] != '\0' ? name_var :
		    scriptname, pf, sizeof(pf));
		if (job_set_str(&job->shim_pidfile, pf, sizeof(pf)) == -1) {
			job_free(job);
			return (NULL);
		}
	}

	/* Label: org.midnightbsd.rc.<scriptname> */
	snprintf(job->label, sizeof(job->label),
	    "org.midnightbsd.rc.%s", scriptname);

	/* A rejected native job can reserve the label without registering. */
	if (unit_label_reserved(job->label)) {
		prowl_log(LOG_DEBUG, "rcshim: native unit reserves %s, skipping %s",
		    job->label, path);
		job_free(job);
		return (NULL);
	}
	if (job_find_by_label(job->label) != NULL) {
		job_free(job);
		return (NULL);
	}

	{
		char desc[PROWL_DESC_MAX];

		snprintf(desc, sizeof(desc), "rc.d compatibility shim for %s",
		    scriptname);
		if (job_set_str(&job->description, desc, sizeof(desc)) == -1) {
			job_free(job);
			return (NULL);
		}
	}

	job->type = JOB_TYPE_RCSHIM;
	strlcpy(job->rc_name, provides_raw[0], sizeof(job->rc_name));

	/* rc-shim invocation: /path/to/script start/stop */
	if (job_set_str(&job->rcshim_path, path, PROWL_PATH_MAX) == -1 ||
	    job_set_str(&job->program, path, PROWL_PATH_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

	/*
	 * provides: all tokens from PROVIDE header; deps: all REQUIRE tokens
	 * are hard dependencies; before: BEFORE tokens.  tokenize_into()
	 * kept each count within its PROWL_*_MAX limit.
	 */
	if (provides_count > 0)
		job->provides = calloc((size_t)provides_count,
		    sizeof(*job->provides));
	if (requires_count > 0)
		job->deps = calloc((size_t)requires_count, sizeof(*job->deps));
	if (before_count > 0)
		job->before = calloc((size_t)before_count,
		    sizeof(*job->before));
	if ((provides_count > 0 && job->provides == NULL) ||
	    (requires_count > 0 && job->deps == NULL) ||
	    (before_count > 0 && job->before == NULL)) {
		prowl_log(LOG_ERR, "rcshim: %s: %m", path);
		job_free(job);
		return (NULL);
	}

	for (int i = 0; i < provides_count; i++)
		strlcpy(job->provides[i], provides_raw[i], PROWL_LABEL_MAX);
	job->provides_count = provides_count;

	for (int i = 0; i < requires_count; i++) {
		strlcpy(job->deps[i].name, requires_raw[i], PROWL_LABEL_MAX);
		job->deps[i].hard = true;
	}
	job->deps_count = requires_count;

	for (int i = 0; i < before_count; i++)
		strlcpy(job->before[i], before_raw[i], PROWL_LABEL_MAX);
	job->before_count = before_count;

	job->shutdown_wait = keyword_shutdown;
	/* KEYWORD: interactive scripts may prompt on the console while booting */
	job->interactive = keyword_interactive;
	/* rcorder -s nojail: scripts that cannot work in a jail are skipped */
	if (keyword_nojail && g_jailed)
		keyword_nostart = true;

	/*
	 * Enabled state.  A script that names an rcvar is enabled only when
	 * that variable is YES in rc.conf (default NO).  A script without an
	 * rcvar (hostname, FILESYSTEMS, mountcritlocal, ...) has no enable
	 * switch and always runs, as it does under rc(8); the pseudo-targets
	 * that other services REQUIRE depend on this.  KEYWORD: nostart
	 * scripts are never run at boot.
	 */
	if (keyword_nostart)
		job->enabled = false;
	else if (!has_rcvar)
		job->enabled = true;
	else if (rcvar_name[0] != '\0')
		job->enabled = rcconf_var_is_enabled(rcvar_name, false);
	else
		job->enabled = rcconf_is_enabled(provides_raw[0], false);

	/*
	 * Only auto-start shims when prowld is the init process.  Under
	 * init(8) /etc/rc has already run these scripts; starting them a
	 * second time would run every service twice.  They stay loaded and
	 * can be driven with prowlctl.
	 */
	job->run_at_load = job->enabled && g_boot_mode;

	return (job);
}

/*
 * Recheck the script and its parents before executing it. The validated
 * parent chain prevents unprivileged pathname replacement. Keep argv[0]
 * and kernel interpreter handling for rc scripts that depend on $0.
 */
int
rcshim_exec(char *const *argv, char *const *envp)
{
	int fd;

	fd = trusted_open_file(argv[0], true);
	if (fd == -1) {
		prowl_log(LOG_WARNING, "rcshim: %s: %m", argv[0]);
		return (-1);
	}
	close(fd);
	return (execve(argv[0], argv, envp));
}

static int
rcshim_script_entry(const struct dirent *de)
{
	return (de->d_name[0] != '.');
}

int
rcshim_scan_dir(const char *dirpath)
{
	struct dirent **names;
	char path[PROWL_PATH_MAX];
	int dfd, count, i, loaded = 0;

	dfd = trusted_open_dir(dirpath);
	if (dfd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "rcshim_scan_dir %s: %m",
			    dirpath);
		return (0);
	}
	/* Sorted, so the order of equal-priority shims does not vary. */
	count = fdscandir(dfd, &names, rcshim_script_entry, alphasort);
	if (count == -1) {
		prowl_log(LOG_WARNING, "rcshim_scan_dir %s: %m", dirpath);
		close(dfd);
		return (0);
	}

	for (i = 0; i < count; i++) {
		const char *name = names[i]->d_name;
		job_t *job;
		int fd;

		if (snprintf(path, sizeof(path), "%s/%s", dirpath, name) >=
		    (int)sizeof(path))
			continue;

		fd = trusted_open_file_at(dfd, name, true);
		if (fd == -1) {
			if (errno != ENOENT && errno != ENOEXEC &&
			    errno != EFTYPE && errno != ELOOP)
				prowl_log(LOG_WARNING, "rcshim: %s: %m", path);
			continue;
		}
		job = rcshim_parse_script(fd, path, name);
		if (job != NULL) {
			TAILQ_INSERT_TAIL(&g_jobs, job, entries);
			prowl_log(LOG_DEBUG, "rcshim loaded %s from %s",
			    job->label, path);
			loaded++;
		}
	}
	for (i = 0; i < count; i++)
		free(names[i]);
	free(names);
	close(dfd);

	if (loaded > 0)
		prowl_log(LOG_NOTICE, "loaded %d rc-shim(s) from %s",
		    loaded, dirpath);

	return (loaded);
}

/*
 * Read rc.conf and rc.conf.local for <name>_enable.
 * Returns the value found, or default_val if not set.
 */
bool
rcconf_is_enabled(const char *name, bool default_val)
{
	char varname[PROWL_LABEL_MAX + sizeof("_enable")];

	snprintf(varname, sizeof(varname), "%s_enable", name);
	return (rcconf_var_is_enabled(varname, default_val));
}

static const char *const rcconf_files[] = {
	"/etc/defaults/rc.conf", RCCONF_PATH, RCCONF_LOCAL_PATH, NULL
};

/* Take ownership of the descriptor returned by a trusted open. */
static FILE *
rcconf_stream(int fd, const char *path)
{
	FILE *fp;

	if (fd >= 0) {
		fp = fdopen(fd, "r");
		if (fp != NULL)
			return (fp);
		prowl_log(LOG_WARNING, "rc.conf: fdopen %s: %m", path);
		close(fd);
	} else if (errno != ENOENT)
		prowl_log(LOG_WARNING, "rc.conf: skipping %s: %m", path);
	return (NULL);
}

static bool
rcconf_close(FILE *fp, const char *path)
{
	int error = 0;

	if (ferror(fp))
		error = errno != 0 ? errno : EIO;
	if (fclose(fp) != 0 && error == 0)
		error = errno;
	if (error != 0)
		prowl_log(LOG_WARNING, "rc.conf: reading %s: %s", path,
		    strerror(error));
	return (error == 0);
}

/* Commit the last assignment only after the entire file was read. */
static bool
rcconf_read(const char *path, int fd, const char *varname, char *buf, size_t sz)
{
	FILE *fp;
	char line[1024], value[sizeof(line)];
	bool found = false;
	size_t n, vlen = strlen(varname);

	fp = rcconf_stream(fd, path);
	if (fp == NULL)
		return (false);
	while (fgets(line, sizeof(line), fp) != NULL) {
		const char *p = skip_ws(line);
		char end = '#';
		bool quoted;

		if (strncmp(p, varname, vlen) != 0)
			continue;
		p = skip_ws(p + vlen);
		if (*p != '=')
			continue;
		p = skip_ws(p + 1);
		quoted = *p == '"' || *p == '\'';
		if (quoted)
			end = *p++;
		n = 0;
		while (p[n] != '\0' && p[n] != '\n' && p[n] != end)
			n++;
		while (!quoted && n > 0 &&
		    (p[n - 1] == ' ' || p[n - 1] == '\t'))
			n--;
		memcpy(value, p, n);
		value[n] = '\0';
		found = true;
	}
	if (!rcconf_close(fp, path))
		found = false;
	if (found)
		strlcpy(buf, value, sz);
	return (found);
}

bool
rcconf_var_get(const char *varname, char *buf, size_t sz)
{
	bool found = false;
	int i;

	if (sz == 0)
		return (false);
	buf[0] = '\0';
	for (i = 0; rcconf_files[i] != NULL; i++) {
		if (rcconf_read(rcconf_files[i],
		    trusted_open_file(rcconf_files[i], false), varname, buf, sz))
			found = true;
	}
	return (found);
}

/* True for the values rc.subr's checkyesno takes as yes */
bool
rcconf_value_yes(const char *val)
{
	return (strcasecmp(val, "yes") == 0 || strcasecmp(val, "true") == 0 ||
	    strcasecmp(val, "on") == 0 || strcmp(val, "1") == 0);
}

/* Match visible rc.conf.d fragments, as in rc.subr's shell glob. */
static int
rcconf_fragment(const struct dirent *entry)
{
	return (entry->d_name[0] != '.' && entry->d_type != DT_DIR);
}

static bool
rcconf_service_file(const char *path, const char *varname, char *buf, size_t sz)
{
	struct dirent **entries;
	char fragment[PROWL_PATH_MAX];
	int dirfd, count, i, n;
	bool found = false;

	dirfd = trusted_open_dir(path);
	if (dirfd == -1) {
		if (errno == ENOTDIR)
			return (rcconf_read(path, trusted_open_file(path, false),
			    varname, buf, sz));
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "rc.conf: skipping %s: %m", path);
		return (false);
	}
	count = fdscandir(dirfd, &entries, rcconf_fragment, alphasort);
	if (count == -1) {
		prowl_log(LOG_WARNING, "rc.conf: scanning %s: %m", path);
		close(dirfd);
		return (false);
	}
	for (i = 0; i < count; i++) {
		n = snprintf(fragment, sizeof(fragment), "%s/%s", path,
		    entries[i]->d_name);
		if (n < 0 || (size_t)n >= sizeof(fragment))
			prowl_log(LOG_WARNING, "rc.conf: filename too long "
			    "in %s", path);
		else if (rcconf_read(fragment, trusted_open_file_at(dirfd,
		    entries[i]->d_name, false), varname, buf, sz))
			found = true;
		free(entries[i]);
	}
	free(entries);
	close(dirfd);
	return (found);
}

/* Per-service files and sorted directory fragments override global values. */
bool
rcconf_job_var(const char *rc_name, const char *varname, char *buf, size_t sz)
{
	static const char *const dirs[] = {
		"/etc/rc.conf.d", "/usr/local/etc/rc.conf.d", NULL
	};
	char path[PROWL_PATH_MAX];
	bool found;
	int i, n;

	found = rcconf_var_get(varname, buf, sz);
	if (sz == 0 || rc_name == NULL || rc_name[0] == '\0')
		return (found);
	if (strchr(rc_name, '/') != NULL || strcmp(rc_name, ".") == 0 ||
	    strcmp(rc_name, "..") == 0) {
		prowl_log(LOG_WARNING, "rc.conf: invalid service name %.128s",
		    rc_name);
		return (found);
	}
	for (i = 0; dirs[i] != NULL; i++) {
		n = snprintf(path, sizeof(path), "%s/%s", dirs[i], rc_name);
		if (n < 0 || (size_t)n >= sizeof(path)) {
			prowl_log(LOG_WARNING, "rc.conf: service name too long");
			break;
		}
		if (rcconf_service_file(path, varname, buf, sz))
			found = true;
	}
	return (found);
}

static bool
append_str(char *out, size_t outsz, size_t *len, const char *s, size_t n)
{
	if (*len + n >= outsz)
		return (false);
	memcpy(out + *len, s, n);
	*len += n;
	out[*len] = '\0';
	return (true);
}

/*
 * Expand @name@, @name:-default@ and @name?text@ in in, using the rc.conf
 * variables of the service rc_name.  @name@ is the variable's value (empty
 * if it is not set), @name:-default@ is the default when it is empty,
 * @name?text@ is text when the variable is YES, TRUE, ON or 1 and nothing
 * otherwise, and @@ is a literal @.  An @ that does not start one of these
 * is left as it is.  Returns the length of the result, or -1 if it does
 * not fit.
 */
int
rcconf_expand(const char *rc_name, const char *in, char *out, size_t outsz)
{
	char name[128], val[PROWL_PATH_MAX];
	const char *p = in, *end, *sep;
	size_t len = 0, nlen;

	if (outsz == 0)
		return (-1);
	out[0] = '\0';

	while (*p != '\0') {
		if (*p != '@') {
			if (!append_str(out, outsz, &len, p, 1))
				return (-1);
			p++;
			continue;
		}
		if (p[1] == '@') {
			if (!append_str(out, outsz, &len, "@", 1))
				return (-1);
			p += 2;
			continue;
		}
		end = strchr(p + 1, '@');
		nlen = 0;
		while (p[1 + nlen] != '\0' &&
		    (isalnum((unsigned char)p[1 + nlen]) ||
		    p[1 + nlen] == '_'))
			nlen++;
		if (end == NULL || nlen == 0 || nlen >= sizeof(name) ||
		    p + 1 + nlen > end) {
			if (!append_str(out, outsz, &len, p, 1))
				return (-1);
			p++;
			continue;
		}
		memcpy(name, p + 1, nlen);
		name[nlen] = '\0';
		sep = p + 1 + nlen;	/* "@", ":-default@" or "?text@" */
		val[0] = '\0';
		(void)rcconf_job_var(rc_name, name, val, sizeof(val));

		if (sep == end) {
			if (!append_str(out, outsz, &len, val, strlen(val)))
				return (-1);
		} else if (sep[0] == ':' && sep[1] == '-') {
			if (val[0] != '\0') {
				if (!append_str(out, outsz, &len, val,
				    strlen(val)))
					return (-1);
			} else if (!append_str(out, outsz, &len, sep + 2,
			    (size_t)(end - (sep + 2))))
				return (-1);
		} else if (sep[0] == ':' && sep[1] == '+') {
			if (val[0] != '\0' && !append_str(out, outsz, &len,
			    sep + 2, (size_t)(end - (sep + 2))))
				return (-1);
		} else if (sep[0] == '?') {
			if (rcconf_value_yes(val) &&
			    !append_str(out, outsz, &len, sep + 1,
			    (size_t)(end - (sep + 1))))
				return (-1);
		} else {
			/* not one of the forms: keep the @ and go on */
			if (!append_str(out, outsz, &len, p, 1))
				return (-1);
			p++;
			continue;
		}
		p = end + 1;
	}
	return ((int)len);
}

/*
 * Expand an argument vector.  An argument that is a single @name@ reference
 * becomes as many arguments as the value has words (none if it is empty),
 * the way an unquoted $flags does in sh; any other argument is expanded in
 * place.  Arguments with a newline are shell scripts and are left alone.
 * The results are malloc'd strings in out, which has room for max
 * pointers; the caller is about to exec, so they are not freed.  Returns
 * the number of arguments, or -1 on error.
 */
int
rcconf_expand_args(const char *rc_name, char *const *in, int n, char **out,
    int max)
{
	char buf[PROWL_PATH_MAX];
	const char *s, *e;
	int i, count = 0;

	for (i = 0; i < n; i++) {
		s = in[i];
		if (strchr(s, '@') == NULL || strchr(s, '\n') != NULL) {
			if (count >= max)
				return (-1);
			out[count++] = (char *)(uintptr_t)s;
			continue;
		}
		if (rcconf_expand(rc_name, s, buf, sizeof(buf)) == -1)
			return (-1);
		e = strchr(s + 1, '@');
		if (s[0] == '@' && e != NULL && e[1] == '\0') {
			/* a lone reference: split into words */
			char *w = buf, *tok;

			while ((tok = strsep(&w, " \t")) != NULL) {
				if (*tok == '\0')
					continue;
				if (count >= max ||
				    (out[count] = strdup(tok)) == NULL)
					return (-1);
				count++;
			}
		} else {
			if (count >= max || (out[count] = strdup(buf)) == NULL)
				return (-1);
			count++;
		}
	}
	return (count);
}

bool
rcconf_var_is_enabled(const char *varname, bool default_val)
{
	FILE *fp;
	char line[1024];
	bool result = default_val;

	for (int fi = 0; rcconf_files[fi] != NULL; fi++) {
		bool enabled = result;

		fp = rcconf_stream(trusted_open_file(rcconf_files[fi], false),
		    rcconf_files[fi]);
		if (fp == NULL)
			continue;

		while (fgets(line, sizeof(line), fp) != NULL) {
			const char *p = skip_ws(line);

			if (*p == '#' || *p == '\n' || *p == '\0')
				continue;

			size_t vlen = strlen(varname);
			if (strncasecmp(p, varname, vlen) != 0)
				continue;

			p += vlen;
			p = skip_ws(p);

			if (*p != '=')
				continue;
			p++;
			p = skip_ws(p);

			/* Strip optional quotes */
			if (*p == '"' || *p == '\'')
				p++;

			if (strncasecmp(p, "YES", 3) == 0 &&
			    (p[3] == '\0' || p[3] == '"' ||
			    p[3] == '\'' || isspace((unsigned char)p[3]))) {
				enabled = true;
			} else if (strncasecmp(p, "NO", 2) == 0 &&
			    (p[2] == '\0' || p[2] == '"' ||
			    p[2] == '\'' || isspace((unsigned char)p[2]))) {
				enabled = false;
			}
		}
		if (rcconf_close(fp, rcconf_files[fi]))
			result = enabled;
	}

	return (result);
}

