/*	$NetBSD: efun.c,v 1.4 2006/09/27 16:20:03 christos Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#include <sys/cdefs.h>

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <libutil.h>

static void (*efunc)(int, const char *, ...) = err;

void (*
esetfunc(void (*ef)(int, const char *, ...)))(int, const char *, ...)
{
	void (*of)(int, const char *, ...) = efunc;
	efunc = ef == NULL ? (void (*)(int, const char *, ...))exit : ef;
	return of;
}

size_t
estrlcpy(char *dst, const char *src, size_t len)
{
	size_t rv;
	if ((rv = strlcpy(dst, src, len)) >= len) {
		errno = ENAMETOOLONG;
		(*efunc)(1,
		    "Cannot copy string; %zu chars needed %zu provided",
		    rv, len);
	}
	return rv;
}

size_t
estrlcat(char *dst, const char *src, size_t len)
{
	size_t rv;
	if ((rv = strlcat(dst, src, len)) >= len) {
		errno = ENAMETOOLONG;
		(*efunc)(1,
		    "Cannot append to string; %zu chars needed %zu provided",
		    rv, len);
	}
	return rv;
}

char *
estrdup(const char *s)
{
	char *d = strdup(s);
	if (d == NULL)
		(*efunc)(1, "Cannot copy string");
	return d;
}

void *
emalloc(size_t n)
{
	void *p = malloc(n);
	if (p == NULL)
		(*efunc)(1, "Cannot allocate %zu bytes", n);
	return p;
}

void *
ecalloc(size_t n, size_t s)
{
	void *p = calloc(n, s);
	if (p == NULL)
		(*efunc)(1, "Cannot allocate %zu bytes", n);
	return p;
}

void *
erealloc(void *p, size_t n)
{
	void *q = realloc(p, n);
	if (q == NULL)
		(*efunc)(1, "Cannot re-allocate %zu bytes", n);
	return q;
}

FILE *
efopen(const char *p, const char *m)
{
	FILE *fp = fopen(p, m);
	if (fp == NULL)
		(*efunc)(1, "Cannot open `%s'", p);
	return fp;
}

#ifdef __BSD_VISIBLE

int
easprintf(char **restrict ret, const char *restrict format, ...)
{
	int rv;
	va_list ap;
	va_start(ap, format);
	if ((rv = vasprintf(ret, format, ap)) == -1)
		(*efunc)(1, "Cannot format string");
	va_end(ap);
	return rv;
}

int
evasprintf(char **restrict ret, const char *restrict format, va_list ap)
{
	int rv;
	if ((rv = vasprintf(ret, format, ap)) == -1)
		(*efunc)(1, "Cannot format string");
	return rv;
}

#endif
