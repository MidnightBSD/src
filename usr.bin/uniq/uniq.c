/*
 * Copyright (c) 1989, 1993
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
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)uniq.c	8.3 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD: release/7.0.0/usr.bin/uniq/uniq.c 169639 2007-05-17 00:19:56Z jmallett $";
#endif /* not lint */

#include <ctype.h>
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

int cflag, dflag, uflag;
int numchars, numfields, repeats;

FILE	*file(const char *, const char *);
wchar_t	*getline(wchar_t *, size_t, FILE *);
void	 show(FILE *, wchar_t *);
wchar_t	*skip(wchar_t *);
void	 obsolete(char *[]);
static void	 usage(void);
int      wcsicoll(wchar_t *, wchar_t *);

int
main (int argc, char *argv[])
{
	wchar_t *t1, *t2;
	FILE *ifp, *ofp;
	int ch;
	wchar_t *prevline, *thisline;
	char *p;
	const char *ifn;
	int iflag = 0, comp;

	(void) setlocale(LC_ALL, "");

	obsolete(argv);
	while ((ch = getopt(argc, argv, "cdif:s:u")) != -1)
		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'f':
			numfields = strtol(optarg, &p, 10);
			if (numfields < 0 || *p)
				errx(1, "illegal field skip value: %s", optarg);
			break;
		case 's':
			numchars = strtol(optarg, &p, 10);
			if (numchars < 0 || *p)
				errx(1, "illegal character skip value: %s", optarg);
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	/* If no flags are set, default is -d -u. */
	if (cflag) {
		if (dflag || uflag)
			usage();
	} else if (!dflag && !uflag)
		dflag = uflag = 1;

	if (argc > 2)
		usage();

	ifp = stdin;
	ifn = "stdin";
	ofp = stdout;
	if (argc > 0 && strcmp(argv[0], "-") != 0)
		ifp = file(ifn = argv[0], "r");
	if (argc > 1)
		ofp = file(argv[1], "w");

	prevline = malloc(MAXLINELEN * sizeof(*prevline));
	thisline = malloc(MAXLINELEN * sizeof(*thisline));
	if (prevline == NULL || thisline == NULL)
		err(1, "malloc");

	if (getline(prevline, MAXLINELEN, ifp) == NULL) {
		if (ferror(ifp))
			err(1, "%s", ifn);
		exit(0);
	}
	if (!cflag && uflag && dflag)
		show(ofp, prevline);

	while (getline(thisline, MAXLINELEN, ifp)) {
		/* If requested get the chosen fields + character offsets. */
		if (numfields || numchars) {
			t1 = skip(thisline);
			t2 = skip(prevline);
		} else {
			t1 = thisline;
			t2 = prevline;
		}

		/* If different, print; set previous to new value. */
		if (iflag)
			comp = wcsicoll(t1, t2);
		else
			comp = wcscoll(t1, t2);

		if (comp) {
			if (cflag || !dflag || !uflag)
				show(ofp, prevline);
			t1 = prevline;
			prevline = thisline;
			if (!cflag && uflag && dflag)
				show(ofp, prevline);
			thisline = t1;
			repeats = 0;
		} else
			++repeats;
	}
	if (ferror(ifp))
		err(1, "%s", ifn);
	if (cflag || !dflag || !uflag)
		show(ofp, prevline);
	exit(0);
}

wchar_t *
getline(wchar_t *buf, size_t buflen, FILE *fp)
{
	size_t bufpos;
	wint_t ch;

	bufpos = 0;
	while (bufpos + 2 != buflen && (ch = getwc(fp)) != WEOF && ch != '\n')
		buf[bufpos++] = ch;
	if (bufpos + 1 != buflen)
		buf[bufpos] = '\0';
	while (ch != WEOF && ch != '\n')
		ch = getwc(fp);

	return (bufpos != 0 || ch == '\n' ? buf : NULL);
}

/*
 * show --
 *	Output a line depending on the flags and number of repetitions
 *	of the line.
 */
void
show(FILE *ofp, wchar_t *str)
{

	if (cflag)
		(void)fprintf(ofp, "%4d %ls\n", repeats + 1, str);
	if ((dflag && repeats) || (uflag && !repeats))
		(void)fprintf(ofp, "%ls\n", str);
}

wchar_t *
skip(wchar_t *str)
{
	int nchars, nfields;

	for (nfields = 0; *str != '\0' && nfields++ != numfields; ) {
		while (iswblank(*str))
			str++;
		while (*str != '\0' && !iswblank(*str))
			str++;
	}
	for (nchars = numchars; nchars-- && *str; ++str);
	return(str);
}

FILE *
file(const char *name, const char *mode)
{
	FILE *fp;

	if ((fp = fopen(name, mode)) == NULL)
		err(1, "%s", name);
	return(fp);
}

void
obsolete(char *argv[])
{
	int len;
	char *ap, *p, *start;

	while ((ap = *++argv)) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-')
			return;
		if (!isdigit((unsigned char)ap[1]))
			continue;
		/*
		 * Digit signifies an old-style option.  Malloc space for dash,
		 * new option and argument.
		 */
		len = strlen(ap);
		if ((start = p = malloc(len + 3)) == NULL)
			err(1, "malloc");
		*p++ = '-';
		*p++ = ap[0] == '+' ? 's' : 'f';
		(void)strcpy(p, ap + 1);
		*argv = start;
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: uniq [-c | -d | -u] [-i] [-f fields] [-s chars] [input [output]]\n");
	exit(1);
}

int
wcsicoll(wchar_t *s1, wchar_t *s2)
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
