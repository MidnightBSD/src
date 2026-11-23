/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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
 * prowl-generator(8) - Generate prowld unit files from rc.d scripts.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define PROWL_LABEL_MAX 256
#define PROWL_DEPS_MAX 32

/* rc.d script header tokens */
#define RCD_PROVIDE "# PROVIDE:"
#define RCD_PROVIDES "# PROVIDES:"
#define RCD_REQUIRE "# REQUIRE:"
#define RCD_REQUIRES "# REQUIRES:"
#define RCD_BEFORE "# BEFORE:"
#define RCD_KEYWORD "# KEYWORD:"
#define RCD_KEYWORDS "# KEYWORDS:"
#define RCD_RCVAR "rcvar="

typedef struct {
	char label[PROWL_LABEL_MAX];
	char rc_name[PROWL_LABEL_MAX];
	char provides[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int provides_count;
	char
		requires[
		    PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int requires_count;
	char before[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int before_count;
	char program[1024];
	bool shutdown_wait;
	bool run_at_load;
} unit_info_t;

static void
usage(void)
{
	fprintf(stderr, "usage: prowl-generator <rc.d script>\n");
	exit(1);
}

/* Skip leading whitespace and return a pointer into s. */
static const char *
skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

static int
tokenize_into(const char *line, char dest[][PROWL_LABEL_MAX], int cur, int max,
    const char *field)
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
		while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' &&
		    *p != '#' && i < sizeof(buf) - 1) {
			buf[i++] = *p++;
		}
		if (i == sizeof(buf) - 1 && *p != '\0' && *p != ' ' &&
		    *p != '\t' && *p != '\n' && *p != '#')
			warnx("%s: token longer than %zu characters truncated",
			    field, sizeof(buf) - 1);
		buf[i] = '\0';

		if (i > 0) {
			strncpy(dest[n], buf, PROWL_LABEL_MAX - 1);
			dest[n][PROWL_LABEL_MAX - 1] = '\0';
			n++;
		}
	}
	if (n == max) {
		p = skip_ws(p);
		if (*p != '\0' && *p != '\n' && *p != '#')
			warnx("%s: more than %d tokens, extras ignored", field,
			    max);
	}
	return (n);
}

/*
 * Label syntax accepted by prowld: non-empty, shorter than
 * PROWL_LABEL_MAX, no leading '.', only alphanumerics and '.', '-', '_',
 * '@', and no "..".
 */
static bool
label_valid(const char *label)
{
	const char *p;
	size_t len;

	if (label == NULL || *label == '\0')
		return (false);
	len = strlen(label);
	if (len >= PROWL_LABEL_MAX)
		return (false);
	if (label[0] == '.')
		return (false);
	for (p = label; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!isalnum(c) && c != '.' && c != '-' && c != '_' && c != '@')
			return (false);
	}
	if (strstr(label, "..") != NULL)
		return (false);
	return (true);
}

/*
 * Look for an "rcvar=" assignment at the start of a script line (p must
 * already be past leading whitespace).  *found is set when the script
 * names a non-empty rcvar.  An empty "rcvar=" means the service has no
 * enable switch.  name receives the variable name when it is a plain
 * literal (e.g. "syslogd_enable"); it is left empty when the value needs
 * shell expansion (e.g. "${name}_enable"), in which case the caller
 * falls back to <PROVIDE>_enable.  Only the first rcvar= line is used.
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
		return; /* empty rcvar: no enable switch */

	*found = true;
	name[0] = '\0';
	for (q = p; *q != '\0' && *q != '"' && *q != '\'' && *q != '\n' &&
	    *q != ' ' && *q != '\t' && *q != ';';
	    q++) {
		if (!isalnum((unsigned char)*q) && *q != '_')
			return; /* needs expansion: caller uses fallback */
	}
	n = (size_t)(q - p);
	if (n == 0 || n >= namesz)
		return;
	memcpy(name, p, n);
	name[n] = '\0';
}

/*
 * Read /etc/defaults/rc.conf, /etc/rc.conf and /etc/rc.conf.local, in
 * order, for varname (e.g. "syslogd_enable").  Later files override
 * earlier ones.  Returns the value found, or default_val if not set.
 */
static bool
rcconf_var_is_enabled(const char *varname, bool default_val)
{
	const char *files[] = { "/etc/defaults/rc.conf", "/etc/rc.conf",
		"/etc/rc.conf.local", NULL };
	FILE *fp;
	char line[1024];
	bool result = default_val;

	for (int fi = 0; files[fi] != NULL; fi++) {
		fp = fopen(files[fi], "r");
		if (fp == NULL)
			continue;

		while (fgets(line, sizeof(line), fp) != NULL) {
			const char *p = skip_ws(line);
			size_t vlen = strlen(varname);

			if (*p == '#' || *p == '\n' || *p == '\0')
				continue;
			if (strncasecmp(p, varname, vlen) != 0)
				continue;

			p = skip_ws(p + vlen);
			if (*p != '=')
				continue;
			p = skip_ws(p + 1);

			/* Strip optional quotes */
			if (*p == '"' || *p == '\'')
				p++;

			if (strncasecmp(p, "YES", 3) == 0 &&
			    (p[3] == '\0' || p[3] == '"' || p[3] == '\'' ||
				isspace((unsigned char)p[3])))
				result = true;
			else if (strncasecmp(p, "NO", 2) == 0 &&
			    (p[2] == '\0' || p[2] == '"' || p[2] == '\'' ||
				isspace((unsigned char)p[2])))
				result = false;
		}
		fclose(fp);
	}

	return (result);
}

static bool
rcconf_is_enabled(const char *name, bool default_val)
{
	char varname[PROWL_LABEL_MAX + sizeof("_enable")];

	snprintf(varname, sizeof(varname), "%s_enable", name);
	return (rcconf_var_is_enabled(varname, default_val));
}

/*
 * Write a JSON-escaped version of src to stdout, surrounded by double quotes.
 * Handles \, ", and control characters.
 */
static void
print_json_string(const char *src)
{
	const unsigned char *p = (const unsigned char *)src;

	putchar('"');
	while (*p != '\0') {
		unsigned char c = *p++;
		if (c == '"' || c == '\\')
			printf("\\%c", c);
		else if (c == '\n')
			printf("\\n");
		else if (c == '\r')
			printf("\\r");
		else if (c == '\t')
			printf("\\t");
		else if (c < 0x20)
			printf("\\u%04x", c);
		else
			putchar(c);
	}
	putchar('"');
}

static void
generate_unit(const unit_info_t *info)
{
	char desc[PROWL_LABEL_MAX + 32];

	snprintf(desc, sizeof(desc), "Generated from rc.d script %s",
	    info->rc_name);

	printf("{\n");
	printf("  \"label\": ");
	print_json_string(info->label);
	printf(",\n  \"description\": ");
	print_json_string(desc);
	printf(",\n  \"program\": ");
	print_json_string(info->program);
	printf(",\n  \"arguments\": [\"start\"],\n");
	printf("  \"type\": \"oneshot\",\n");
	printf("  \"rc_name\": ");
	print_json_string(info->rc_name);
	printf(",\n");

	if (info->provides_count > 1) {
		printf("  \"provides\": [");
		for (int i = 0; i < info->provides_count; i++) {
			print_json_string(info->provides[i]);
			if (i < info->provides_count - 1)
				printf(", ");
		}
		printf("],\n");
	}

	if (info->requires_count > 0) {
		printf("  \"requires\": [");
		for (int i = 0; i < info->requires_count; i++) {
			print_json_string(info->requires[i]);
			if (i < info->requires_count - 1)
				printf(", ");
		}
		printf("],\n");
	}

	if (info->before_count > 0) {
		printf("  \"before\": [");
		for (int i = 0; i < info->before_count; i++) {
			print_json_string(info->before[i]);
			if (i < info->before_count - 1)
				printf(", ");
		}
		printf("],\n");
	}

	if (info->shutdown_wait)
		printf("  \"shutdown_wait\": true,\n");

	printf("  \"run_at_load\": %s\n", info->run_at_load ? "true" : "false");
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	struct stat sb;
	FILE *fp;
	char line[1024];
	unit_info_t info;
	char *path, *base, *scriptname;
	bool done_headers = false;
	bool has_rcvar = false;
	bool keyword_nostart = false;
	char rcvar_name[PROWL_LABEL_MAX];
	int fd;

	if (argc != 2)
		usage();

	path = argv[1];
	/* O_NONBLOCK so a fifo cannot block the open; it is ignored for
	 * regular files, which the fstat below requires. */
	fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd == -1)
		err(1, "cannot open %s", path);

	/*
	 * Only generate units from executable regular files that are
	 * root-owned and not writable by group or other, mirroring the
	 * checks prowld applies before running an rc.d script.  This also
	 * rejects directories, fifos and devices up front.
	 */
	if (fstat(fd, &sb) == -1)
		err(1, "cannot stat %s", path);
	if (!S_ISREG(sb.st_mode))
		errx(1, "%s: not a regular file", path);
	if (sb.st_uid != 0 || (sb.st_mode & (S_IWGRP | S_IWOTH)) != 0)
		errx(1, "%s: not owned by root or writable by group/other",
		    path);
	if ((sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
		errx(1, "%s: not executable", path);

	fp = fdopen(fd, "r");
	if (fp == NULL)
		err(1, "fdopen %s", path);

	memset(&info, 0, sizeof(info));

	/* Get absolute path for program if possible */
	if (path[0] == '/') {
		if (strlen(path) >= sizeof(info.program))
			warnx("%s: program path truncated to %zu characters",
			    path, sizeof(info.program) - 1);
		strncpy(info.program, path, sizeof(info.program) - 1);
	} else {
		if (realpath(path, info.program) == NULL) {
			if (strlen(path) >= sizeof(info.program))
				warnx("%s: program path truncated to %zu "
				      "characters",
				    path, sizeof(info.program) - 1);
			strncpy(info.program, path, sizeof(info.program) - 1);
		}
	}
	info.program[sizeof(info.program) - 1] = '\0';

	base = strdup(path);
	if (base == NULL)
		err(1, "strdup");
	scriptname = basename(base);
	if (!label_valid(scriptname))
		errx(1, "%s: script name is not valid for a unit label",
		    scriptname);
	strncpy(info.rc_name, scriptname, PROWL_LABEL_MAX - 1);
	if (snprintf(info.label, sizeof(info.label), "org.midnightbsd.rc.%s",
		scriptname) >= (int)sizeof(info.label))
		errx(1, "%s: label too long for unit label prefix", scriptname);
	free(base);

	rcvar_name[0] = '\0';
	while (fgets(line, sizeof(line), fp) != NULL) {
		const char *p = line;

		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\0' || *p == '\n')
			continue;

		/*
		 * Past the header comments, only the rcvar= assignment is
		 * still interesting: it names the rc.conf enable switch.
		 */
		if (done_headers || *p != '#') {
			done_headers = true;
			rcvar_scan(p, &has_rcvar, rcvar_name,
			    sizeof(rcvar_name));
			continue;
		}

		if (strncmp(p, RCD_PROVIDE, strlen(RCD_PROVIDE)) == 0)
			info.provides_count = tokenize_into(p +
				strlen(RCD_PROVIDE),
			    info.provides, info.provides_count, PROWL_DEPS_MAX,
			    "PROVIDE");
		else if (strncmp(p, RCD_PROVIDES, strlen(RCD_PROVIDES)) == 0)
			info.provides_count = tokenize_into(p +
				strlen(RCD_PROVIDES),
			    info.provides, info.provides_count, PROWL_DEPS_MAX,
			    "PROVIDES");
		else if (strncmp(p, RCD_REQUIRE, strlen(RCD_REQUIRE)) == 0)
			info.requires_count = tokenize_into(p +
				strlen(RCD_REQUIRE),
			    info.requires, info.requires_count, PROWL_DEPS_MAX,
			    "REQUIRE");
		else if (strncmp(p, RCD_REQUIRES, strlen(RCD_REQUIRES)) == 0)
			info.requires_count = tokenize_into(p +
				strlen(RCD_REQUIRES),
			    info.requires, info.requires_count, PROWL_DEPS_MAX,
			    "REQUIRES");
		else if (strncmp(p, RCD_BEFORE, strlen(RCD_BEFORE)) == 0)
			info.before_count = tokenize_into(p +
				strlen(RCD_BEFORE),
			    info.before, info.before_count, PROWL_DEPS_MAX,
			    "BEFORE");
		else if (strncmp(p, RCD_KEYWORD, strlen(RCD_KEYWORD)) == 0 ||
		    strncmp(p, RCD_KEYWORDS, strlen(RCD_KEYWORDS)) == 0) {
			const char *kw = (strncmp(p, RCD_KEYWORD,
					      strlen(RCD_KEYWORD)) == 0) ?
			    p + strlen(RCD_KEYWORD) :
			    p + strlen(RCD_KEYWORDS);
			if (strstr(kw, "shutdown") != NULL)
				info.shutdown_wait = true;
			if (strstr(kw, "nostart") != NULL)
				keyword_nostart = true;
		}
	}
	fclose(fp);

	if (info.provides_count > 0) {
		strncpy(info.rc_name, info.provides[0], PROWL_LABEL_MAX - 1);
	}

	/*
	 * Gate run_at_load the way prowld's rc-shim does: a KEYWORD:
	 * nostart script never runs at load, a script without an rcvar
	 * always does, and otherwise the rcvar (or <PROVIDE>_enable) must
	 * be YES in rc.conf.
	 */
	if (keyword_nostart)
		info.run_at_load = false;
	else if (!has_rcvar)
		info.run_at_load = true;
	else if (rcvar_name[0] != '\0')
		info.run_at_load = rcconf_var_is_enabled(rcvar_name, false);
	else
		info.run_at_load = rcconf_is_enabled(info.rc_name, false);

	generate_unit(&info);

	return (0);
}
