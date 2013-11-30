/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code was contributed to The NetBSD Foundation by Allen Briggs.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*	$NetBSD: fmtcheck.c,v 1.2 2000/11/01 01:17:20 briggs Exp $	*/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/fmtcheck.c,v 1.8 2005/03/21 08:00:55 das Exp $");

#include <stdio.h>
#include <string.h>
#include <ctype.h>

__weak_reference(__fmtcheck, fmtcheck);

enum __e_fmtcheck_types {
	FMTCHECK_START,
	FMTCHECK_SHORT,
	FMTCHECK_INT,
	FMTCHECK_LONG,
	FMTCHECK_QUAD,
	FMTCHECK_PTRDIFFT,
	FMTCHECK_SIZET,
	FMTCHECK_SHORTPOINTER,
	FMTCHECK_INTPOINTER,
	FMTCHECK_LONGPOINTER,
	FMTCHECK_QUADPOINTER,
	FMTCHECK_PTRDIFFTPOINTER,
	FMTCHECK_SIZETPOINTER,
#ifndef NO_FLOATING_POINT
	FMTCHECK_DOUBLE,
	FMTCHECK_LONGDOUBLE,
#endif
	FMTCHECK_STRING,
	FMTCHECK_WIDTH,
	FMTCHECK_PRECISION,
	FMTCHECK_DONE,
	FMTCHECK_UNKNOWN
};
typedef enum __e_fmtcheck_types EFT;

#define RETURN(pf,f,r) do { \
			*(pf) = (f); \
			return r; \
		       } /*NOTREACHED*/ /*CONSTCOND*/ while (0)

static EFT
get_next_format_from_precision(const char **pf)
{
	int		sh, lg, quad, longdouble, ptrdifft, sizet;
	const char	*f;

	sh = lg = quad = longdouble = ptrdifft = sizet = 0;

	f = *pf;
	switch (*f) {
	case 'h':
		f++;
		sh = 1;
		break;
	case 'l':
		f++;
		if (!*f) RETURN(pf,f,FMTCHECK_UNKNOWN);
		if (*f == 'l') {
			f++;
			quad = 1;
		} else {
			lg = 1;
		}
		break;
	case 'q':
		f++;
		quad = 1;
		break;
	case 't':
		f++;
		ptrdifft = 1;
		break;
	case 'z':
		f++;
		sizet = 1;
		break;
	case 'L':
		f++;
		longdouble = 1;
		break;
	default:
		break;
	}
	if (!*f) RETURN(pf,f,FMTCHECK_UNKNOWN);
	if (strchr("diouxX", *f)) {
		if (longdouble)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		if (lg)
			RETURN(pf,f,FMTCHECK_LONG);
		if (quad)
			RETURN(pf,f,FMTCHECK_QUAD);
		if (ptrdifft)
			RETURN(pf,f,FMTCHECK_PTRDIFFT);
		if (sizet)
			RETURN(pf,f,FMTCHECK_SIZET);
		RETURN(pf,f,FMTCHECK_INT);
	}
	if (*f == 'n') {
		if (longdouble)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		if (sh)
			RETURN(pf,f,FMTCHECK_SHORTPOINTER);
		if (lg)
			RETURN(pf,f,FMTCHECK_LONGPOINTER);
		if (quad)
			RETURN(pf,f,FMTCHECK_QUADPOINTER);
		if (ptrdifft)
			RETURN(pf,f,FMTCHECK_PTRDIFFTPOINTER);
		if (sizet)
			RETURN(pf,f,FMTCHECK_SIZETPOINTER);
		RETURN(pf,f,FMTCHECK_INTPOINTER);
	}
	if (strchr("DOU", *f)) {
		if (sh + lg + quad + longdouble + ptrdifft + sizet)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		RETURN(pf,f,FMTCHECK_LONG);
	}
#ifndef NO_FLOATING_POINT
	if (strchr("aAeEfFgG", *f)) {
		if (longdouble)
			RETURN(pf,f,FMTCHECK_LONGDOUBLE);
		if (sh + lg + quad + ptrdifft + sizet)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		RETURN(pf,f,FMTCHECK_DOUBLE);
	}
#endif
	if (*f == 'c') {
		if (sh + lg + quad + longdouble + ptrdifft + sizet)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		RETURN(pf,f,FMTCHECK_INT);
	}
	if (*f == 's') {
		if (sh + lg + quad + longdouble + ptrdifft + sizet)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		RETURN(pf,f,FMTCHECK_STRING);
	}
	if (*f == 'p') {
		if (sh + lg + quad + longdouble + ptrdifft + sizet)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		RETURN(pf,f,FMTCHECK_LONG);
	}
	RETURN(pf,f,FMTCHECK_UNKNOWN);
	/*NOTREACHED*/
}

static EFT
get_next_format_from_width(const char **pf)
{
	const char	*f;

	f = *pf;
	if (*f == '.') {
		f++;
		if (*f == '*') {
			RETURN(pf,f,FMTCHECK_PRECISION);
		}
		/* eat any precision (empty is allowed) */
		while (isdigit(*f)) f++;
		if (!*f) RETURN(pf,f,FMTCHECK_UNKNOWN);
	}
	RETURN(pf,f,get_next_format_from_precision(pf));
	/*NOTREACHED*/
}

static EFT
get_next_format(const char **pf, EFT eft)
{
	int		infmt;
	const char	*f;

	if (eft == FMTCHECK_WIDTH) {
		(*pf)++;
		return get_next_format_from_width(pf);
	} else if (eft == FMTCHECK_PRECISION) {
		(*pf)++;
		return get_next_format_from_precision(pf);
	}

	f = *pf;
	infmt = 0;
	while (!infmt) {
		f = strchr(f, '%');
		if (f == NULL)
			RETURN(pf,f,FMTCHECK_DONE);
		f++;
		if (!*f)
			RETURN(pf,f,FMTCHECK_UNKNOWN);
		if (*f != '%')
			infmt = 1;
		else
			f++;
	}

	/* Eat any of the flags */
	while (*f && (strchr("#'0- +", *f)))
		f++;

	if (*f == '*') {
		RETURN(pf,f,FMTCHECK_WIDTH);
	}
	/* eat any width */
	while (isdigit(*f)) f++;
	if (!*f) {
		RETURN(pf,f,FMTCHECK_UNKNOWN);
	}

	RETURN(pf,f,get_next_format_from_width(pf));
	/*NOTREACHED*/
}

__const char *
__fmtcheck(const char *f1, const char *f2)
{
	const char	*f1p, *f2p;
	EFT		f1t, f2t;

	if (!f1) return f2;
	
	f1p = f1;
	f1t = FMTCHECK_START;
	f2p = f2;
	f2t = FMTCHECK_START;
	while ((f1t = get_next_format(&f1p, f1t)) != FMTCHECK_DONE) {
		if (f1t == FMTCHECK_UNKNOWN)
			return f2;
		f2t = get_next_format(&f2p, f2t);
		if (f1t != f2t)
			return f2;
	}
	return f1;
}
