/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#if 0
#ifndef lint
static char sccsid[] = "From: @(#)comm.c	8.4 (Berkeley) 5/4/95";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/usr.bin/comm/comm.c 131497 2004-07-02 22:48:29Z tjr $");

#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define	MAXLINELEN	(LINE_MAX + 1)

const wchar_t *tabs[] = { L"", L"\t", L"\t\t" };

FILE   *file(const char *);
void	show(FILE *, const char *, const wchar_t *, wchar_t *);
int     wcsicoll(const wchar_t *, const wchar_t *);
static void	usage(void);

int
main(int argc, char *argv[])
{
	int comp, file1done = 0, file2done = 0, read1, read2;
	int ch, flag1, flag2, flag3, iflag;
	FILE *fp1, *fp2;
	const wchar_t *col1, *col2, *col3;
	wchar_t line1[MAXLINELEN], line2[MAXLINELEN];
	const wchar_t **p;

	flag1 = flag2 = flag3 = 1;
	iflag = 0;

	(void) setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "123i")) != -1)
		switch(ch) {
		case '1':
			flag1 = 0;
			break;
		case '2':
			flag2 = 0;
			break;
		case '3':
			flag3 = 0;
			break;
		case 'i':
			iflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	fp1 = file(argv[0]);
	fp2 = file(argv[1]);

	/* for each column printed, add another tab offset */
	p = tabs;
	col1 = col2 = col3 = NULL;
	if (flag1)
		col1 = *p++;
	if (flag2)
		col2 = *p++;
	if (flag3)
		col3 = *p;

	for (read1 = read2 = 1;;) {
		/* read next line, check for EOF */
		if (read1) {
			file1done = !fgetws(line1, MAXLINELEN, fp1);
			if (file1done && ferror(fp1))
				err(1, "%s", argv[0]);
		}
		if (read2) {
			file2done = !fgetws(line2, MAXLINELEN, fp2);
			if (file2done && ferror(fp2))
				err(1, "%s", argv[1]);
		}

		/* if one file done, display the rest of the other file */
		if (file1done) {
			if (!file2done && col2)
				show(fp2, argv[1], col2, line2);
			break;
		}
		if (file2done) {
			if (!file1done && col1)
				show(fp1, argv[0], col1, line1);
			break;
		}

		/* lines are the same */
		if(iflag)
			comp = wcsicoll(line1, line2);
		else
			comp = wcscoll(line1, line2);

		if (!comp) {
			read1 = read2 = 1;
			if (col3)
				(void)printf("%ls%ls", col3, line1);
			continue;
		}

		/* lines are different */
		if (comp < 0) {
			read1 = 1;
			read2 = 0;
			if (col1)
				(void)printf("%ls%ls", col1, line1);
		} else {
			read1 = 0;
			read2 = 1;
			if (col2)
				(void)printf("%ls%ls", col2, line2);
		}
	}
	exit(0);
}

void
show(FILE *fp, const char *fn, const wchar_t *offset, wchar_t *buf)
{

	do {
		(void)printf("%ls%ls", offset, buf);
	} while (fgetws(buf, MAXLINELEN, fp));
	if (ferror(fp))
		err(1, "%s", fn);
}

FILE *
file(const char *name)
{
	FILE *fp;

	if (!strcmp(name, "-"))
		return (stdin);
	if ((fp = fopen(name, "r")) == NULL) {
		err(1, "%s", name);
	}
	return (fp);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: comm [-123i] file1 file2\n");
	exit(1);
}

int
wcsicoll(const wchar_t *s1, const wchar_t *s2)
{
	wchar_t *p, line1[MAXLINELEN], line2[MAXLINELEN];

	for (p = line1; *s1; s1++)
		*p++ = towlower(*s1);
	*p = '\0';
	for (p = line2; *s2; s2++)
		*p++ = towlower(*s2);
	*p = '\0';
	return (wcscoll(line1, line2));
}
