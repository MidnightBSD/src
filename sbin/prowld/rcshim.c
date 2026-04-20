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
rcshim_parse_script(const char *path, const char *scriptname)
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
	bool done_headers = false;

	{
		int rfd = open(path, O_RDONLY | O_NOFOLLOW);
		if (rfd == -1) {
			prowl_log(LOG_WARNING, "rcshim: cannot open %s: %m",
			    path);
			return (NULL);
		}
		fp = fdopen(rfd, "r");
		if (fp == NULL) {
			prowl_log(LOG_WARNING, "rcshim: fdopen %s: %m", path);
			close(rfd);
			return (NULL);
		}
	}

	/*
	 * rc.d headers appear before the first non-comment, non-blank line.
	 * We stop scanning once we hit executable code.
	 */
	while (!done_headers && fgets(line, sizeof(line), fp) != NULL) {
		const char *p = line;

		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\0' || *p == '\n')
			continue;

		if (*p != '#') {
			done_headers = true;
			break;
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
	if (job_find_by_provides(provides_raw[0]) != NULL) {
		prowl_log(LOG_DEBUG, "rcshim: native unit covers %s, "
		    "skipping %s", provides_raw[0], path);
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

	/* Label: org.midnightbsd.rc.<scriptname> */
	snprintf(job->label, sizeof(job->label),
	    "org.midnightbsd.rc.%s", scriptname);

	/* If already registered, skip */
	if (job_find_by_label(job->label) != NULL) {
		job_free(job);
		return (NULL);
	}

	snprintf(job->description, sizeof(job->description),
	    "rc.d compatibility shim for %s", scriptname);

	job->type = JOB_TYPE_RCSHIM;
	strlcpy(job->rc_name, provides_raw[0], sizeof(job->rc_name));
	strlcpy(job->rcshim_path, path, sizeof(job->rcshim_path));

	/* rc-shim invocation: /path/to/script start/stop */
	strlcpy(job->program, path, sizeof(job->program));

	/* provides: all tokens from PROVIDE header */
	for (int i = 0; i < provides_count && i < PROWL_PROVIDES_MAX; i++)
		strlcpy(job->provides[i], provides_raw[i], PROWL_LABEL_MAX);
	job->provides_count = provides_count;

	/* deps: all REQUIRE tokens are hard dependencies */
	for (int i = 0; i < requires_count && i < PROWL_DEPS_MAX; i++) {
		strlcpy(job->deps[i].name, requires_raw[i], PROWL_LABEL_MAX);
		job->deps[i].hard = true;
	}
	job->deps_count = requires_count;

	/* before */
	for (int i = 0; i < before_count && i < PROWL_DEPS_MAX; i++)
		strlcpy(job->before[i], before_raw[i], PROWL_LABEL_MAX);
	job->before_count = before_count;

	job->shutdown_wait = keyword_shutdown;
	(void)keyword_nojail; /* TODO: add nojail condition check */

	/* Enabled state: default to false for base system services */
	job->enabled = rcconf_is_enabled(provides_raw[0], false);
	job->run_at_load = job->enabled;

	return (job);
}

int
rcshim_scan_dir(const char *dirpath)
{
	DIR *dir;
	struct dirent *de;
	char path[PROWL_PATH_MAX];
	struct stat sb;
	int loaded = 0;

	dir = opendir(dirpath);
	if (dir == NULL) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "rcshim_scan_dir %s: %m",
			    dirpath);
		return (0);
	}

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.')
			continue;

		snprintf(path, sizeof(path), "%s/%s", dirpath, de->d_name);

		/*
		 * Use lstat(2) so symlinks are seen as symlinks, not as the
		 * files they point to.  A symlink is not a regular file and
		 * will be skipped by the S_ISREG check below.
		 */
		if (lstat(path, &sb) == -1)
			continue;

		/* Skip directories, symlinks, and non-regular files */
		if (!S_ISREG(sb.st_mode))
			continue;

		/* Only load executable scripts */
		if ((sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
			continue;

		/*
		 * Apply the same trust checks as unit_load_file(): the script
		 * must be owned by root and must not be world-writable.
		 * rc-shims are executed as root, so a non-root-owned or
		 * world-writable script is an arbitrary-code-execution risk.
		 */
		if (sb.st_uid != 0) {
			prowl_log(LOG_WARNING,
			    "rcshim: %s not owned by root (uid %u), skipping",
			    path, (unsigned)sb.st_uid);
			continue;
		}
		if (sb.st_mode & S_IWOTH) {
			prowl_log(LOG_WARNING,
			    "rcshim: %s is world-writable, skipping", path);
			continue;
		}

		job_t *job = rcshim_parse_script(path, de->d_name);
		if (job != NULL) {
			TAILQ_INSERT_TAIL(&g_jobs, job, entries);
			prowl_log(LOG_DEBUG, "rcshim loaded %s from %s",
			    job->label, path);
			loaded++;
		}
	}

	closedir(dir);

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
	const char *files[] = {
		"/etc/defaults/rc.conf",
		RCCONF_PATH,
		RCCONF_LOCAL_PATH,
		NULL
	};
	FILE *fp;
	char line[1024];
	char varname[PROWL_LABEL_MAX];
	bool result = default_val;

	snprintf(varname, sizeof(varname), "%s_enable", name);

	for (int fi = 0; files[fi] != NULL; fi++) {
		fp = fopen(files[fi], "r");
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
				result = true;
			} else if (strncasecmp(p, "NO", 2) == 0 &&
			    (p[2] == '\0' || p[2] == '"' ||
			    p[2] == '\'' || isspace((unsigned char)p[2]))) {
				result = false;
			}
		}
		fclose(fp);
	}

	return (result);
}

