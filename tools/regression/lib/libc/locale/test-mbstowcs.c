/*-
 * Copyright (c) 2002 Tim J. Robbins
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
 * Test program for mbstowcs(), as specified by IEEE Std. 1003.1-2001 and
 * ISO/IEC 9899:1999.
 *
 * The function is tested with both the "C" ("POSIX") LC_CTYPE setting and
 * "ja_JP.eucJP". Other encodings are not tested.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int
main(int argc, char *argv[])
{
	char srcbuf[128];
	wchar_t dstbuf[128];

	/*
	 * C/POSIX locale.
	 */

	printf("1..1\n");

	/* Simple null terminated string. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	assert(mbstowcs(dstbuf, srcbuf, sizeof(dstbuf) / sizeof(*dstbuf)) == 5);
	assert(wcscmp(dstbuf, L"hello") == 0);
	assert(dstbuf[6] == 0xcccc);

	/* Not enough space in destination buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	assert(mbstowcs(dstbuf, srcbuf, 4) == 4);
	assert(wmemcmp(dstbuf, L"hell", 4) == 0);
	assert(dstbuf[5] == 0xcccc);

	/* Null terminated string, internal dest. buffer (XSI extension) */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	assert(mbstowcs(NULL, srcbuf, 0) == 5);

	/* Empty source buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	srcbuf[0] = '\0';
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	assert(mbstowcs(dstbuf, srcbuf, 1) == 0);
	assert(dstbuf[0] == 0);
	assert(dstbuf[1] == 0xcccc);

	/* Zero length destination buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	assert(mbstowcs(dstbuf, srcbuf, 0) == 0);
	assert(dstbuf[0] == 0xcccc);

	/*
	 * Japanese (EUC) locale.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "ja_JP.eucJP"), "ja_JP.eucJP") == 0);
	assert(MB_CUR_MAX > 1);

	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "\xA3\xC1 B \xA3\xC3");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	assert(mbstowcs(dstbuf, srcbuf, sizeof(dstbuf) / sizeof(*dstbuf)) == 5);
	assert(dstbuf[0] == 0xA3C1 && dstbuf[1] == 0x20 && dstbuf[2] == 0x42 &&
	    dstbuf[3] == 0x20 && dstbuf[4] == 0xA3C3 && dstbuf[5] == 0);

	printf("ok 1 - mbstowcs()\n");

	return (0);
}
