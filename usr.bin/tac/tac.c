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
#include <libcasper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct line {
	char *data;
	size_t len;
};

static int rval;
static fileargs_t *fa;

static void usage(void) __dead2;
static void scanfiles(char *[]);
static void tac(FILE *, const char *);

static void
usage(void)
{

	fprintf(stderr, "usage: tac [file ...]\n");
	exit(1);
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
	struct line *lines, *newlines;
	char *line;
	size_t cap, nlines, size;
	ssize_t nread;

	lines = NULL;
	cap = nlines = 0;
	line = NULL;
	size = 0;

	while ((nread = getline(&line, &size, fp)) != -1) {
		if (nlines == cap) {
			if (cap > SIZE_MAX / 2)
				errx(1, "too many lines");
			cap = cap == 0 ? 128 : cap * 2;
			newlines = reallocarray(lines, cap, sizeof(*lines));
			if (newlines == NULL)
				err(1, NULL);
			lines = newlines;
		}

		lines[nlines].len = (size_t)nread;
		lines[nlines].data = malloc(lines[nlines].len);
		if (lines[nlines].data == NULL)
			err(1, NULL);
		memcpy(lines[nlines].data, line, lines[nlines].len);
		nlines++;
	}
	free(line);

	if (ferror(fp)) {
		warn("%s", path);
		rval = 1;
	}

	while (nlines > 0) {
		nlines--;
		if (fwrite(lines[nlines].data, lines[nlines].len, 1, stdout) !=
		    1)
			err(1, "stdout");
		free(lines[nlines].data);
	}
	free(lines);
}

int
main(int argc, char *argv[])
{
	cap_rights_t rights;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

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

	return (rval);
}
