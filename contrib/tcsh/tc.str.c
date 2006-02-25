/* $Header: /home/cvs/src/contrib/tcsh/tc.str.c,v 1.1.1.2 2006-02-25 02:34:05 laffer1 Exp $ */
/*
 * tc.str.c: Short string package
 * 	     This has been a lesson of how to write buggy code!
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include "sh.h"

#include <limits.h>

RCSID("$Id: tc.str.c,v 1.1.1.2 2006-02-25 02:34:05 laffer1 Exp $")

#define MALLOC_INCR	128
#ifdef WIDE_STRINGS
#define MALLOC_SURPLUS	MB_LEN_MAX /* Space for one multibyte character */
#else
#define MALLOC_SURPLUS	0
#endif

#ifdef WIDE_STRINGS
size_t
one_mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    int len;

    len = rt_mbtowc(pwc, s, n);
    if (len == -1) {
        mbtowc(NULL, NULL, 0);
	*pwc = (unsigned char)*s | INVALID_BYTE;
    }
    if (len <= 0)
	len = 1;
    return len;
}

size_t
one_wctomb(char *s, wchar_t wchar)
{
    int len;

    if (wchar & INVALID_BYTE) {
	s[0] = wchar & 0xFF;
	len = 1;
    } else {
	len = wctomb(s, wchar);
	if (len == -1)
	    s[0] = wchar;
	if (len <= 0)
	    len = 1;
    }
    return len;
}
#endif
     
#ifdef SHORT_STRINGS
int
rt_mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    int ret;
    char back[MB_LEN_MAX];

    ret = mbtowc(pwc, s, n);
    if (ret > 0 && (wctomb(back, *pwc) != ret || memcmp(s, back, ret) != 0))
	ret = -1;
    return ret;
}

Char  **
blk2short(src)
    char **src;
{
    size_t     n;
    Char **sdst, **dst;

    /*
     * Count
     */
    for (n = 0; src[n] != NULL; n++)
	continue;
    sdst = dst = (Char **) xmalloc((size_t) ((n + 1) * sizeof(Char *)));

    for (; *src != NULL; src++)
	*dst++ = SAVE(*src);
    *dst = NULL;
    return (sdst);
}

char  **
short2blk(src)
    Char **src;
{
    size_t     n;
    char **sdst, **dst;

    /*
     * Count
     */
    for (n = 0; src[n] != NULL; n++)
	continue;
    sdst = dst = (char **) xmalloc((size_t) ((n + 1) * sizeof(char *)));

    for (; *src != NULL; src++)
	*dst++ = strsave(short2str(*src));
    *dst = NULL;
    return (sdst);
}

Char   *
str2short(src)
    const char *src;
{
    static Char *sdst;
    static size_t dstsize = 0;
    Char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == (NULL)) {
	dstsize = MALLOC_INCR;
	sdst = (Char *) xmalloc((size_t) (dstsize * sizeof(Char)));
    }

    dst = sdst;
    edst = &dst[dstsize];
    while ((unsigned char) *src) {
	src += one_mbtowc(dst, src, MB_LEN_MAX);
	dst++;
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = (Char *) xrealloc((ptr_t) sdst,
				     (size_t) (dstsize * sizeof(Char)));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

char   *
short2str(src)
    const Char *src;
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == NULL) {
	dstsize = MALLOC_INCR;
	sdst = (char *) xmalloc((size_t) ((dstsize + MALLOC_SURPLUS)
					  * sizeof(char)));
    }
    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {
	dst += one_wctomb(dst, *src & CHAR);
	src++;
	if (dst >= edst) {
	    dstsize += MALLOC_INCR;
	    sdst = (char *) xrealloc((ptr_t) sdst,
				     (size_t) ((dstsize + MALLOC_SURPLUS)
					       * sizeof(char)));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

#ifndef WIDE_STRINGS
Char   *
s_strcpy(dst, src)
    Char *dst;
    const Char *src;
{
    Char *sdst;

    sdst = dst;
    while ((*dst++ = *src++) != '\0')
	continue;
    return (sdst);
}

Char   *
s_strncpy(dst, src, n)
    Char *dst;
    const Char *src;
    size_t n;
{
    Char *sdst;

    if (n == 0)
	return(dst);

    sdst = dst;
    do 
	if ((*dst++ = *src++) == '\0') {
	    while (--n != 0)
		*dst++ = '\0';
	    return(sdst);
	}
    while (--n != 0);
    return (sdst);
}

Char   *
s_strcat(dst, src)
    Char *dst;
    const Char *src;
{
    Char *sdst;

    sdst = dst;
    while (*dst++)
	continue;
    --dst;
    while ((*dst++ = *src++) != '\0')
	continue;
    return (sdst);
}

#ifdef NOTUSED
Char   *
s_strncat(dst, src, n)
    Char *dst;
    const Char *src;
    size_t n;
{
    Char *sdst;

    if (n == 0) 
	return (dst);

    sdst = dst;

    while (*dst++)
	continue;
    --dst;

    do 
	if ((*dst++ = *src++) == '\0')
	    return(sdst);
    while (--n != 0)
	continue;

    *dst = '\0';
    return (sdst);
}

#endif

Char   *
s_strchr(str, ch)
    const Char *str;
    int ch;
{
    do
	if (*str == ch)
	    return ((Char *)(intptr_t)str);
    while (*str++);
    return (NULL);
}

Char   *
s_strrchr(str, ch)
    const Char *str;
    int ch;
{
    const Char *rstr;

    rstr = NULL;
    do
	if (*str == ch)
	    rstr = str;
    while (*str++);
    return ((Char *)(intptr_t)rstr);
}

size_t
s_strlen(str)
    const Char *str;
{
    size_t n;

    for (n = 0; *str++; n++)
	continue;
    return (n);
}

int
s_strcmp(str1, str2)
    const Char *str1, *str2;
{
    for (; *str1 && *str1 == *str2; str1++, str2++)
	continue;
    /*
     * The following case analysis is necessary so that characters which look
     * negative collate low against normal characters but high against the
     * end-of-string NUL.
     */
    if (*str1 == '\0' && *str2 == '\0')
	return (0);
    else if (*str1 == '\0')
	return (-1);
    else if (*str2 == '\0')
	return (1);
    else
	return (*str1 - *str2);
}

int
s_strncmp(str1, str2, n)
    const Char *str1, *str2;
    size_t n;
{
    if (n == 0)
	return (0);
    do {
	if (*str1 != *str2) {
	    /*
	     * The following case analysis is necessary so that characters 
	     * which look negative collate low against normal characters
	     * but high against the end-of-string NUL.
	     */
	    if (*str1 == '\0')
		return (-1);
	    else if (*str2 == '\0')
		return (1);
	    else
		return (*str1 - *str2);
	}
        if (*str1 == '\0')
	    return(0);
	str1++, str2++;
    } while (--n != 0);
    return(0);
}
#endif /* not WIDE_STRINGS */

int
s_strcasecmp(str1, str2)
    const Char *str1, *str2;
{
#ifdef WIDE_STRINGS
    wchar_t l1 = 0, l2 = 0;
    for (; *str1 && ((*str1 == *str2 && (l1 = l2 = 0) == 0) || 
	(l1 = towlower(*str1)) == (l2 = towlower(*str2))); str1++, str2++)
	continue;
    
#else
    unsigned char c1, c2, l1 = 0, l2 = 0;
    for (; *str1 && ((*str1 == *str2 && (l1 = l2 = 0) == 0) || 
	((c1 = (unsigned char)*str1) == *str1 &&
	 (c2 = (unsigned char)*str2) == *str2 &&
	(l1 = tolower(c1)) == (l2 = tolower(c2)))); str1++, str2++)
	continue;
#endif
    /*
     * The following case analysis is necessary so that characters which look
     * negative collate low against normal characters but high against the
     * end-of-string NUL.
     */
    if (*str1 == '\0' && *str2 == '\0')
	return (0);
    else if (*str1 == '\0')
	return (-1);
    else if (*str2 == '\0')
	return (1);
    else if (l1 == l2)	/* They are zero when they are equal */
	return (*str1 - *str2);
    else
	return (l1 - l2);
}

Char   *
s_strsave(s)
    const Char *s;
{
    Char   *n;
    Char *p;

    if (s == 0)
	s = STRNULL;
    for (p = (Char *)(intptr_t)s; *p++;)
	continue;
    n = p = (Char *) xmalloc((size_t) 
			     ((((const Char *) p) - s) * sizeof(Char)));
    while ((*p++ = *s++) != '\0')
	continue;
    return (n);
}

Char   *
s_strspl(cp, dp)
    const Char   *cp, *dp;
{
    Char   *ep;
    Char *p, *q;

    if (!cp)
	cp = STRNULL;
    if (!dp)
	dp = STRNULL;
    for (p = (Char *)(intptr_t) cp; *p++;)
	continue;
    for (q = (Char *)(intptr_t) dp; *q++;)
	continue;
    ep = (Char *) xmalloc((size_t)
			  (((((const Char *) p) - cp) + 
			    (((const Char *) q) - dp) - 1) * sizeof(Char)));
    for (p = ep, q = (Char*)(intptr_t) cp; (*p++ = *q++) != '\0';)
	continue;
    for (p--, q = (Char *)(intptr_t) dp; (*p++ = *q++) != '\0';)
	continue;
    return (ep);
}

Char   *
s_strend(cp)
    const Char *cp;
{
    if (!cp)
	return ((Char *)(intptr_t) cp);
    while (*cp)
	cp++;
    return ((Char *)(intptr_t) cp);
}

Char   *
s_strstr(s, t)
    const Char *s, *t;
{
    do {
	const Char *ss = s;
	const Char *tt = t;

	do
	    if (*tt == '\0')
		return ((Char *)(intptr_t) s);
	while (*ss++ == *tt++);
    } while (*s++ != '\0');
    return (NULL);
}

#endif				/* SHORT_STRINGS */

char   *
short2qstr(src)
    const Char *src;
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == NULL) {
	dstsize = MALLOC_INCR;
	sdst = (char *) xmalloc((size_t) ((dstsize + MALLOC_SURPLUS)
					  * sizeof(char)));
    }
    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {
	if (*src & QUOTE) {
	    *dst++ = '\\';
	    if (dst == edst) {
		dstsize += MALLOC_INCR;
		sdst = (char *) xrealloc((ptr_t) sdst,
					 (size_t) ((dstsize + MALLOC_SURPLUS)
						   * sizeof(char)));
		edst = &sdst[dstsize];
		dst = &edst[-MALLOC_INCR];
	    }
	}
	dst += one_wctomb(dst, *src & CHAR);
	src++;
	if (dst >= edst) {
	    dstsize += MALLOC_INCR;
	    sdst = (char *) xrealloc((ptr_t) sdst,
				     (size_t) ((dstsize + MALLOC_SURPLUS)
					       * sizeof(char)));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}
