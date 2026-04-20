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

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROWL_LABEL_MAX 256
#define PROWL_DEPS_MAX  32

/* rc.d script header tokens */
#define RCD_PROVIDE	"# PROVIDE:"
#define RCD_PROVIDES	"# PROVIDES:"
#define RCD_REQUIRE	"# REQUIRE:"
#define RCD_REQUIRES	"# REQUIRES:"
#define RCD_BEFORE	"# BEFORE:"
#define RCD_KEYWORD	"# KEYWORD:"
#define RCD_KEYWORDS	"# KEYWORDS:"

typedef struct {
	char label[PROWL_LABEL_MAX];
	char rc_name[PROWL_LABEL_MAX];
	char provides[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int provides_count;
	char requires[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int requires_count;
	char before[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int before_count;
	char program[1024];
	bool shutdown_wait;
} unit_info_t;

static void
usage(void)
{
	fprintf(stderr, "usage: prowl-generator <rc.d script>\n");
	exit(1);
}

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
			strncpy(dest[n], buf, PROWL_LABEL_MAX - 1);
			dest[n][PROWL_LABEL_MAX - 1] = '\0';
			n++;
		}
	}
	return (n);
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
	printf("{\n");
	printf("  \"label\": ");
	print_json_string(info->label);
	printf(",\n  \"description\": \"Generated from rc.d script ");
	print_json_string(info->rc_name);
	printf("\",\n  \"program\": ");
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

	printf("  \"run_at_load\": true\n");
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char line[1024];
	unit_info_t info;
	char *path, *base, *scriptname;
	bool done_headers = false;

	if (argc != 2)
		usage();

	path = argv[1];
	fp = fopen(path, "r");
	if (fp == NULL)
		err(1, "cannot open %s", path);

	memset(&info, 0, sizeof(info));
	
	/* Get absolute path for program if possible */
	if (path[0] == '/') {
		strncpy(info.program, path, sizeof(info.program) - 1);
	} else {
		if (realpath(path, info.program) == NULL)
			strncpy(info.program, path, sizeof(info.program) - 1);
	}

	base = strdup(path);
	scriptname = basename(base);
	strncpy(info.rc_name, scriptname, PROWL_LABEL_MAX - 1);
	snprintf(info.label, sizeof(info.label), "org.midnightbsd.rc.%s", scriptname);
	free(base);

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
			info.provides_count = tokenize_into(p + strlen(RCD_PROVIDE),
			    info.provides, info.provides_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_PROVIDES, strlen(RCD_PROVIDES)) == 0)
			info.provides_count = tokenize_into(p + strlen(RCD_PROVIDES),
			    info.provides, info.provides_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_REQUIRE, strlen(RCD_REQUIRE)) == 0)
			info.requires_count = tokenize_into(p + strlen(RCD_REQUIRE),
			    info.requires, info.requires_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_REQUIRES, strlen(RCD_REQUIRES)) == 0)
			info.requires_count = tokenize_into(p + strlen(RCD_REQUIRES),
			    info.requires, info.requires_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_BEFORE, strlen(RCD_BEFORE)) == 0)
			info.before_count = tokenize_into(p + strlen(RCD_BEFORE),
			    info.before, info.before_count, PROWL_DEPS_MAX);
		else if (strncmp(p, RCD_KEYWORD, strlen(RCD_KEYWORD)) == 0 ||
		    strncmp(p, RCD_KEYWORDS, strlen(RCD_KEYWORDS)) == 0) {
			const char *kw = (strncmp(p, RCD_KEYWORD, strlen(RCD_KEYWORD)) == 0) ?
			    p + strlen(RCD_KEYWORD) : p + strlen(RCD_KEYWORDS);
			if (strstr(kw, "shutdown") != NULL)
				info.shutdown_wait = true;
		}
	}
	fclose(fp);

	if (info.provides_count > 0) {
		strncpy(info.rc_name, info.provides[0], PROWL_LABEL_MAX - 1);
	}

	generate_unit(&info);

	return (0);
}
