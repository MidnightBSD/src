/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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

#include <sys/types.h>
#include <sys/capsicum.h>

#include <capsicum_helpers.h>
#include <casper/cap_fileargs.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <libcasper.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct record {
	size_t off;
	size_t len;
};

static int rval;
static int rflag;
static int separator_ends_record = 1;
static fileargs_t *fa;
static const char *separator = "\n";
static size_t separator_len = 1;
static const char nul_separator = '\0';
static regex_t separator_re;

static void usage(void) __dead2;
static void append_record(struct record **, size_t *, size_t *, size_t, size_t);
static const char *find_literal(const char *, size_t, const char *, size_t);
static void free_records(char *, struct record *, size_t);
static void init_separator(void);
static void parse_records(char *, size_t, struct record **, size_t *);
static void read_input(FILE *, const char *, char **, size_t *);
static void scanfiles(char *[]);
static void tac(FILE *, const char *);

static const struct option long_opts[] = { { "before", no_argument, NULL, 'b' },
	{ "regex", no_argument, NULL, 'r' },
	{ "separator", required_argument, NULL, 's' }, { NULL, 0, NULL, 0 } };

static void
usage(void)
{

	fprintf(stderr, "usage: tac [-br] [-s separator] [file ...]\n");
	exit(1);
}

static void
append_record(struct record **records, size_t *nrecords, size_t *cap,
    size_t off, size_t len)
{
	struct record *newrecords;

	if (len == 0)
		return;
	if (*nrecords == *cap) {
		if (*cap > SIZE_MAX / 2)
			errx(1, "too many records");
		*cap = *cap == 0 ? 128 : *cap * 2;
		newrecords = reallocarray(*records, *cap, sizeof(*records));
		if (newrecords == NULL)
			err(1, NULL);
		*records = newrecords;
	}
	(*records)[*nrecords].off = off;
	(*records)[*nrecords].len = len;
	(*nrecords)++;
}

static const char *
find_literal(const char *buf, size_t len, const char *sep, size_t seplen)
{
	size_t i;

	if (seplen > len)
		return (NULL);
	for (i = 0; i <= len - seplen; i++) {
		if (memcmp(buf + i, sep, seplen) == 0)
			return (buf + i);
	}
	return (NULL);
}

static void
free_records(char *buf, struct record *records, size_t nrecords)
{
	size_t i;

	for (i = nrecords; i > 0; i--) {
		if (fwrite(buf + records[i - 1].off, records[i - 1].len, 1,
			stdout) != 1)
			err(1, "stdout");
	}
	free(records);
	free(buf);
}

static void
init_separator(void)
{
	if (*separator == '\0') {
		if (rflag)
			errx(1, "separator cannot be empty");
		separator = &nul_separator;
		separator_len = 1;
		return;
	}

	separator_len = strlen(separator);
	if (rflag) {
		int error;

		error = regcomp(&separator_re, separator, 0);
		if (error != 0) {
			char errbuf[128];

			regerror(error, &separator_re, errbuf, sizeof(errbuf));
			errx(1, "%s", errbuf);
		}
	}
}

static void
parse_records(char *buf, size_t len, struct record **records, size_t *nrecords)
{
	regmatch_t match;
	size_t cap, match_len, match_off, record_start, search_start;

	cap = 0;
	record_start = search_start = 0;
	*records = NULL;
	*nrecords = 0;

	while (search_start < len) {
		if (rflag) {
			int error;

			error = regexec(&separator_re, buf + search_start, 1,
			    &match, 0);
			if (error == REG_NOMATCH)
				break;
			if (error != 0)
				errx(1, "regular expression search failed");
			if (match.rm_so == match.rm_eo)
				errx(1, "separator matched empty string");
			match_off = search_start + (size_t)match.rm_so;
			match_len = (size_t)(match.rm_eo - match.rm_so);
		} else {
			const char *matchp;

			matchp = find_literal(buf + search_start,
			    len - search_start, separator, separator_len);
			if (matchp == NULL)
				break;
			match_off = (size_t)(matchp - buf);
			match_len = separator_len;
		}

		if (separator_ends_record) {
			append_record(records, nrecords, &cap, record_start,
			    match_off + match_len - record_start);
			record_start = search_start = match_off + match_len;
		} else {
			append_record(records, nrecords, &cap, record_start,
			    match_off - record_start);
			record_start = match_off;
			search_start = match_off + match_len;
		}
	}

	append_record(records, nrecords, &cap, record_start,
	    len - record_start);
}

static void
read_input(FILE *fp, const char *path, char **buf, size_t *len)
{
	char *newbuf;
	size_t cap;

	*buf = NULL;
	*len = 0;
	cap = 0;

	for (;;) {
		if (*len == cap) {
			if (cap > SIZE_MAX / 2)
				errx(1, "input too large");
			cap = cap == 0 ? 8192 : cap * 2;
			newbuf = realloc(*buf, cap + 1);
			if (newbuf == NULL)
				err(1, NULL);
			*buf = newbuf;
		}

		size_t nread = fread(*buf + *len, 1, cap - *len, fp);

		*len += nread;
		if (nread == 0)
			break;
	}

	if (ferror(fp)) {
		warn("%s", path);
		rval = 1;
	}
	if (*buf != NULL)
		(*buf)[*len] = '\0';
}

static void
scanfiles(char *argv[])
{
	FILE *fp;

	if (*argv == NULL) {
		tac(stdin, "stdin");
		return;
	}

	for (; *argv != NULL; argv++) {
		if (strcmp(*argv, "-") == 0) {
			tac(stdin, "stdin");
			continue;
		}

		fp = fileargs_fopen(fa, *argv, "r");
		if (fp == NULL) {
			warn("%s", *argv);
			rval = 1;
			continue;
		}
		tac(fp, *argv);
		if (fclose(fp) != 0) {
			warn("%s", *argv);
			rval = 1;
		}
	}
}

static void
tac(FILE *fp, const char *path)
{
	struct record *records;
	char *buf;
	size_t len, nrecords;

	read_input(fp, path, &buf, &len);
	parse_records(buf, len, &records, &nrecords);
	free_records(buf, records, nrecords);
}

int
main(int argc, char *argv[])
{
	cap_rights_t rights;
	int ch;

	while ((ch = getopt_long(argc, argv, "brs:", long_opts, NULL)) != -1) {
		switch (ch) {
		case 'b':
			separator_ends_record = 0;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			separator = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	init_separator();

	fa = fileargs_init(argc, argv, O_RDONLY, 0,
	    cap_rights_init(&rights, CAP_READ, CAP_FSTAT, CAP_FCNTL), FA_OPEN);
	if (fa == NULL)
		err(1, "unable to init casper");

	caph_cache_catpages();
	if (caph_limit_stdio() < 0 || caph_enter_casper() < 0)
		err(1, "unable to enter capability mode");

	scanfiles(argv);
	fileargs_free(fa);

	if (fclose(stdout) != 0)
		err(1, "stdout");
	if (rflag)
		regfree(&separator_re);

	return (rval);
}
