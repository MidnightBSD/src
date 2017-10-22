/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
/*
 * Important: This file is used both as a standalone program /usr/bin/printf
 * and as a builtin for /bin/sh (#define SHELL).
 */

#ifndef SHELL
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */
#endif

#ifndef lint
#if 0
static char const sccsid[] = "@(#)printf.c	8.1 (Berkeley) 7/20/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#ifdef SHELL
#define main printfcmd
#include "bltin/bltin.h"
#include "error.h"
#endif

#define PF(f, func) do {						\
	char *b = NULL;							\
	if (havewidth)							\
		if (haveprec)						\
			(void)asprintf(&b, f, fieldwidth, precision, func); \
		else							\
			(void)asprintf(&b, f, fieldwidth, func);	\
	else if (haveprec)						\
		(void)asprintf(&b, f, precision, func);			\
	else								\
		(void)asprintf(&b, f, func);				\
	if (b) {							\
		(void)fputs(b, stdout);					\
		free(b);						\
	}								\
} while (0)

static int	 asciicode(void);
static char	*printf_doformat(char *, int *);
static int	 escape(char *, int, size_t *);
static int	 getchr(void);
static int	 getfloating(long double *, int);
static int	 getint(int *);
static int	 getnum(intmax_t *, uintmax_t *, int);
static const char
		*getstr(void);
static char	*mknum(char *, char);
static void	 usage(void);

static char **gargv;

int
main(int argc, char *argv[])
{
	size_t len;
	int ch, chopped, end, rval;
	char *format, *fmt, *start;

#ifndef SHELL
	(void) setlocale(LC_ALL, "");
#endif
#ifdef SHELL
	optreset = 1; optind = 1; opterr = 0; /* initialize getopt */
#endif
	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			usage();
			return (1);
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		return (1);
	}

#ifdef SHELL
	INTOFF;
#endif
	/*
	 * Basic algorithm is to scan the format string for conversion
	 * specifications -- once one is found, find out if the field
	 * width or precision is a '*'; if it is, gather up value.  Note,
	 * format strings are reused as necessary to use up the provided
	 * arguments, arguments of zero/null string are provided to use
	 * up the format string.
	 */
	fmt = format = *argv;
	chopped = escape(fmt, 1, &len);		/* backslash interpretation */
	rval = end = 0;
	gargv = ++argv;
	for (;;) {
		start = fmt;
		while (fmt < format + len) {
			if (fmt[0] == '%') {
				fwrite(start, 1, fmt - start, stdout);
				if (fmt[1] == '%') {
					/* %% prints a % */
					putchar('%');
					fmt += 2;
				} else {
					fmt = printf_doformat(fmt, &rval);
					if (fmt == NULL) {
#ifdef SHELL
						INTON;
#endif
						return (1);
					}
					end = 0;
				}
				start = fmt;
			} else
				fmt++;
		}

		if (end == 1) {
			warnx("missing format character");
#ifdef SHELL
			INTON;
#endif
			return (1);
		}
		fwrite(start, 1, fmt - start, stdout);
		if (chopped || !*gargv) {
#ifdef SHELL
			INTON;
#endif
			return (rval);
		}
		/* Restart at the beginning of the format string. */
		fmt = format;
		end = 1;
	}
	/* NOTREACHED */
}


static char *
printf_doformat(char *start, int *rval)
{
	static const char skip1[] = "#'-+ 0";
	static const char skip2[] = "0123456789";
	char *fmt;
	int fieldwidth, haveprec, havewidth, mod_ldbl, precision;
	char convch, nextch;

	fmt = start + 1;
	/* skip to field width */
	fmt += strspn(fmt, skip1);
	if (*fmt == '*') {
		if (getint(&fieldwidth))
			return (NULL);
		havewidth = 1;
		++fmt;
	} else {
		havewidth = 0;

		/* skip to possible '.', get following precision */
		fmt += strspn(fmt, skip2);
	}
	if (*fmt == '.') {
		/* precision present? */
		++fmt;
		if (*fmt == '*') {
			if (getint(&precision))
				return (NULL);
			haveprec = 1;
			++fmt;
		} else {
			haveprec = 0;

			/* skip to conversion char */
			fmt += strspn(fmt, skip2);
		}
	} else
		haveprec = 0;
	if (!*fmt) {
		warnx("missing format character");
		return (NULL);
	}

	/*
	 * Look for a length modifier.  POSIX doesn't have these, so
	 * we only support them for floating-point conversions, which
	 * are extensions.  This is useful because the L modifier can
	 * be used to gain extra range and precision, while omitting
	 * it is more likely to produce consistent results on different
	 * architectures.  This is not so important for integers
	 * because overflow is the only bad thing that can happen to
	 * them, but consider the command  printf %a 1.1
	 */
	if (*fmt == 'L') {
		mod_ldbl = 1;
		fmt++;
		if (!strchr("aAeEfFgG", *fmt)) {
			warnx("bad modifier L for %%%c", *fmt);
			return (NULL);
		}
	} else {
		mod_ldbl = 0;
	}

	convch = *fmt;
	nextch = *++fmt;
	*fmt = '\0';
	switch (convch) {
	case 'b': {
		size_t len;
		char *p;
		int getout;

		p = strdup(getstr());
		if (p == NULL) {
			warnx("%s", strerror(ENOMEM));
			return (NULL);
		}
		getout = escape(p, 0, &len);
		*(fmt - 1) = 's';
		PF(start, p);
		*(fmt - 1) = 'b';
		free(p);
		if (getout)
			return (fmt);
		break;
	}
	case 'c': {
		char p;

		p = getchr();
		PF(start, p);
		break;
	}
	case 's': {
		const char *p;

		p = getstr();
		PF(start, p);
		break;
	}
	case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
		char *f;
		intmax_t val;
		uintmax_t uval;
		int signedconv;

		signedconv = (convch == 'd' || convch == 'i');
		if ((f = mknum(start, convch)) == NULL)
			return (NULL);
		if (getnum(&val, &uval, signedconv))
			*rval = 1;
		if (signedconv)
			PF(f, val);
		else
			PF(f, uval);
		break;
	}
	case 'e': case 'E':
	case 'f': case 'F':
	case 'g': case 'G':
	case 'a': case 'A': {
		long double p;

		if (getfloating(&p, mod_ldbl))
			*rval = 1;
		if (mod_ldbl)
			PF(start, p);
		else
			PF(start, (double)p);
		break;
	}
	default:
		warnx("illegal format character %c", convch);
		return (NULL);
	}
	*fmt = nextch;
	return (fmt);
}

static char *
mknum(char *str, char ch)
{
	static char *copy;
	static size_t copy_size;
	char *newcopy;
	size_t len, newlen;

	len = strlen(str) + 2;
	if (len > copy_size) {
		newlen = ((len + 1023) >> 10) << 10;
		if ((newcopy = realloc(copy, newlen)) == NULL)
		{
			warnx("%s", strerror(ENOMEM));
			return (NULL);
		}
		copy = newcopy;
		copy_size = newlen;
	}

	memmove(copy, str, len - 3);
	copy[len - 3] = 'j';
	copy[len - 2] = ch;
	copy[len - 1] = '\0';
	return (copy);
}

static int
escape(char *fmt, int percent, size_t *len)
{
	char *save, *store, c;
	int value;

	for (save = store = fmt; ((c = *fmt) != 0); ++fmt, ++store) {
		if (c != '\\') {
			*store = c;
			continue;
		}
		switch (*++fmt) {
		case '\0':		/* EOS, user error */
			*store = '\\';
			*++store = '\0';
			*len = store - save;
			return (0);
		case '\\':		/* backslash */
		case '\'':		/* single quote */
			*store = *fmt;
			break;
		case 'a':		/* bell/alert */
			*store = '\a';
			break;
		case 'b':		/* backspace */
			*store = '\b';
			break;
		case 'c':
			*store = '\0';
			*len = store - save;
			return (1);
		case 'f':		/* form-feed */
			*store = '\f';
			break;
		case 'n':		/* newline */
			*store = '\n';
			break;
		case 'r':		/* carriage-return */
			*store = '\r';
			break;
		case 't':		/* horizontal tab */
			*store = '\t';
			break;
		case 'v':		/* vertical tab */
			*store = '\v';
			break;
					/* octal constant */
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			c = (!percent && *fmt == '0') ? 4 : 3;
			for (value = 0;
			    c-- && *fmt >= '0' && *fmt <= '7'; ++fmt) {
				value <<= 3;
				value += *fmt - '0';
			}
			--fmt;
			if (percent && value == '%') {
				*store++ = '%';
				*store = '%';
			} else
				*store = (char)value;
			break;
		default:
			*store = *fmt;
			break;
		}
	}
	*store = '\0';
	*len = store - save;
	return (0);
}

static int
getchr(void)
{
	if (!*gargv)
		return ('\0');
	return ((int)**gargv++);
}

static const char *
getstr(void)
{
	if (!*gargv)
		return ("");
	return (*gargv++);
}

static int
getint(int *ip)
{
	intmax_t val;
	uintmax_t uval;
	int rval;

	if (getnum(&val, &uval, 1))
		return (1);
	rval = 0;
	if (val < INT_MIN || val > INT_MAX) {
		warnx("%s: %s", *gargv, strerror(ERANGE));
		rval = 1;
	}
	*ip = (int)val;
	return (rval);
}

static int
getnum(intmax_t *ip, uintmax_t *uip, int signedconv)
{
	char *ep;
	int rval;

	if (!*gargv) {
		*ip = 0;
		return (0);
	}
	if (**gargv == '"' || **gargv == '\'') {
		if (signedconv)
			*ip = asciicode();
		else
			*uip = asciicode();
		return (0);
	}
	rval = 0;
	errno = 0;
	if (signedconv)
		*ip = strtoimax(*gargv, &ep, 0);
	else
		*uip = strtoumax(*gargv, &ep, 0);
	if (ep == *gargv) {
		warnx("%s: expected numeric value", *gargv);
		rval = 1;
	}
	else if (*ep != '\0') {
		warnx("%s: not completely converted", *gargv);
		rval = 1;
	}
	if (errno == ERANGE) {
		warnx("%s: %s", *gargv, strerror(ERANGE));
		rval = 1;
	}
	++gargv;
	return (rval);
}

static int
getfloating(long double *dp, int mod_ldbl)
{
	char *ep;
	int rval;

	if (!*gargv) {
		*dp = 0.0;
		return (0);
	}
	if (**gargv == '"' || **gargv == '\'') {
		*dp = asciicode();
		return (0);
	}
	rval = 0;
	errno = 0;
	if (mod_ldbl)
		*dp = strtold(*gargv, &ep);
	else
		*dp = strtod(*gargv, &ep);
	if (ep == *gargv) {
		warnx("%s: expected numeric value", *gargv);
		rval = 1;
	} else if (*ep != '\0') {
		warnx("%s: not completely converted", *gargv);
		rval = 1;
	}
	if (errno == ERANGE) {
		warnx("%s: %s", *gargv, strerror(ERANGE));
		rval = 1;
	}
	++gargv;
	return (rval);
}

static int
asciicode(void)
{
	int ch;
	wchar_t wch;
	mbstate_t mbs;

	ch = (unsigned char)**gargv;
	if (ch == '\'' || ch == '"') {
		memset(&mbs, 0, sizeof(mbs));
		switch (mbrtowc(&wch, *gargv + 1, MB_LEN_MAX, &mbs)) {
		case (size_t)-2:
		case (size_t)-1:
			wch = (unsigned char)gargv[0][1];
			break;
		case 0:
			wch = 0;
			break;
		}
		ch = wch;
	}
	++gargv;
	return (ch);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: printf format [arguments ...]\n");
}
