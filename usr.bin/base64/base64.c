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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <resolv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENCODE_IN 57
#define ENCODE_OUT (((ENCODE_IN + 2) / 3) * 4 + 1)

static bool dflag;
static bool iflag;
static unsigned long wrap = 76;

static void base64_decode(FILE *, const char *);
static void base64_encode(FILE *, const char *);
static bool base64_char(unsigned char);
static void usage(void) __dead2;

static const struct option long_opts[] = { { "decode", no_argument, NULL, 'd' },
	{ "ignore-garbage", no_argument, NULL, 'i' },
	{ "wrap", required_argument, NULL, 'w' }, { NULL, 0, NULL, 0 } };

static void
usage(void)
{

	fprintf(stderr, "usage: base64 [-di] [-w columns] [file]\n");
	exit(1);
}

static bool
base64_char(unsigned char ch)
{

	return (isalnum(ch) || ch == '+' || ch == '/' || ch == '=');
}

static void
base64_encode(FILE *fp, const char *path)
{
	unsigned char inbuf[ENCODE_IN];
	char outbuf[ENCODE_OUT];
	size_t column, n, off, take;

	column = 0;
	while ((n = fread(inbuf, 1, sizeof(inbuf), fp)) > 0) {
		size_t len;
		int rv;

		rv = b64_ntop(inbuf, n, outbuf, sizeof(outbuf));
		if (rv == -1)
			errx(1, "b64_ntop: error encoding base64");
		len = (size_t)rv;
		for (off = 0; off < len; off += take) {
			if (wrap == 0) {
				take = len - off;
			} else {
				take = wrap - column;
				if (take > len - off)
					take = len - off;
			}
			if (fwrite(outbuf + off, 1, take, stdout) != take)
				err(1, "stdout");
			column += take;
			if (wrap != 0 && column == wrap) {
				if (putchar('\n') == EOF)
					err(1, "stdout");
				column = 0;
			}
		}
	}
	if (ferror(fp))
		err(1, "%s", path);
	if (wrap != 0 && column != 0 && putchar('\n') == EOF)
		err(1, "stdout");
}

static void
base64_decode(FILE *fp, const char *path)
{
	unsigned char *outbuf;
	char *buf, *newbuf;
	size_t cap, len, outcap, i;
	int n;

	buf = NULL;
	cap = len = 0;
	for (;;) {
		if (len == cap) {
			if (cap > SIZE_MAX / 2)
				errx(1, "input too large");
			cap = cap == 0 ? 8192 : cap * 2;
			newbuf = realloc(buf, cap + 1);
			if (newbuf == NULL)
				err(1, NULL);
			buf = newbuf;
		}

		size_t oldlen = len;
		size_t nread = fread(buf + oldlen, 1, cap - oldlen, fp);

		for (i = 0; i < nread; i++) {
			unsigned char ch;

			ch = (unsigned char)buf[oldlen + i];
			if (base64_char(ch)) {
				buf[len++] = (char)ch;
			} else if (!isspace(ch)) {
				if (!iflag)
					errx(1, "%s: invalid input", path);
			}
		}
		if (nread == 0)
			break;
	}
	if (ferror(fp))
		err(1, "%s", path);
	if (buf == NULL)
		return;
	buf[len] = '\0';
	if (len == 0) {
		free(buf);
		return;
	}

	if (len / 4 > (SIZE_MAX - 3) / 3)
		errx(1, "input too large");
	outcap = (len / 4) * 3 + 3;
	outbuf = malloc(outcap);
	if (outbuf == NULL)
		err(1, NULL);

	n = b64_pton(buf, outbuf, outcap);
	if (n < 0)
		errx(1, "%s: invalid input", path);
	/* cppcheck-suppress nullPointerOutOfMemory */
	if (fwrite(outbuf, 1, (size_t)n, stdout) != (size_t)n)
		err(1, "stdout");
	free(outbuf);
	free(buf);
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *endp;
	int ch;

	while ((ch = getopt_long(argc, argv, "diw:", long_opts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			dflag = true;
			break;
		case 'i':
			iflag = true;
			break;
		case 'w':
			errno = 0;
			wrap = strtoul(optarg, &endp, 10);
			if (errno != 0 || *optarg == '\0' || *endp != '\0')
				errx(1, "invalid wrap size: %s", optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	if (argc == 0 || strcmp(argv[0], "-") == 0) {
		fp = stdin;
		if (dflag)
			base64_decode(fp, "stdin");
		else
			base64_encode(fp, "stdin");
	} else {
		fp = fopen(argv[0], "r");
		if (fp == NULL)
			err(1, "%s", argv[0]);
		if (dflag)
			base64_decode(fp, argv[0]);
		else
			base64_encode(fp, argv[0]);
		/* cppcheck-suppress nullPointerOutOfResources */
		if (fclose(fp) != 0)
			err(1, "%s", argv[0]);
	}

	if (fclose(stdout) != 0)
		err(1, "stdout");

	return (0);
}
