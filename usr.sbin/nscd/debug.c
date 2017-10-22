/*-
 * Copyright (c) 2004 Michael Bushkov <bushman@rsu.ru>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/usr.sbin/nscd/debug.c 172344 2007-09-27 12:30:12Z bushman $");

#include <stdio.h>
#include "debug.h"

static	int	trace_level = 0;
static	int	trace_level_bk = 0;

void
__trace_in(const char *s, const char *f, int l)
{
	int i;
	if (trace_level < TRACE_WANTED)
	{
		for (i = 0; i < trace_level; ++i)
			printf("\t");

		printf("=> %s\n", s);
	}

	++trace_level;
}

void
__trace_point(const char *f, int l)
{
	int i;

	if (trace_level < TRACE_WANTED)
	{
		for (i = 0; i < trace_level - 1; ++i)
			printf("\t");

		printf("= %s: %d\n", f, l);
	}
}

void
__trace_msg(const char *msg, const char *f, int l)
{
	int i;

	if (trace_level < TRACE_WANTED)
	{
		for (i = 0; i < trace_level - 1; ++i)
			printf("\t");

		printf("= MSG %s, %s: %d\n", msg, f, l);
	}
}

void
__trace_ptr(const char *desc, const void *p, const char *f, int l)
{
	int i;

	if (trace_level < TRACE_WANTED)
	{
		for (i = 0; i < trace_level - 1; ++i)
			printf("\t");

		printf("= PTR %s: %p, %s: %d\n", desc, p, f, l);
	}
}

void
__trace_int(const char *desc, int i, const char *f, int l)
{
	int j;

	if (trace_level < TRACE_WANTED)
	{
		for (j = 0; j < trace_level - 1; ++j)
			printf("\t");

		printf("= INT %s: %i, %s: %d\n",desc, i, f, l);
	}
}

void
__trace_str(const char *desc, const char *s, const char *f, int l)
{
	int i;

	if (trace_level < TRACE_WANTED)
	{
		for (i = 0; i < trace_level - 1; ++i)
			printf("\t");

		printf("= STR %s: '%s', %s: %d\n", desc, s, f, l);
	}
}

void
__trace_out(const char *s, const char *f, int l)
{
	int i;

	--trace_level;
	if (trace_level < TRACE_WANTED)
	{
		for (i = 0; i < trace_level; ++i)
			printf("\t");

		printf("<= %s\n", s);
	}
}

void
__trace_on()
{
	trace_level = trace_level_bk;
	trace_level_bk = 0;
}

void
__trace_off()
{
	trace_level_bk = trace_level;
	trace_level = 1024;
}
