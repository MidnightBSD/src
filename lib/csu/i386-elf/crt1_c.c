/* LINTLIBRARY */
/*-
 * Copyright 1996-1998 John D. Polstra.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $MidnightBSD: src/lib/csu/i386-elf/crt1_c.c,v 1.1 2011/12/18 02:23:42 laffer1 Exp $
 * $FreeBSD: src/lib/csu/i386-elf/crt1_c.c,v 1.1.4.2 2010/01/19 20:19:52 kib Exp $
 */

#ifndef lint
#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif
#endif /* lint */

#include <stdlib.h>

#include "libc_private.h"
#include "crtbrand.c"

extern int _DYNAMIC;
#pragma weak _DYNAMIC

typedef void (*fptr)(void);

extern void _fini(void);
extern void _init(void);
extern int main(int, char **, char **);
extern void _start(char *, ...);

extern void (*__preinit_array_start []) (int, char **, char **) __dso_hidden;
extern void (*__preinit_array_end   []) (int, char **, char **) __dso_hidden;
extern void (*__init_array_start    []) (int, char **, char **) __dso_hidden;
extern void (*__init_array_end      []) (int, char **, char **) __dso_hidden;
extern void (*__fini_array_start    []) (void) __dso_hidden;
extern void (*__fini_array_end      []) (void) __dso_hidden;

#ifdef GCRT
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

char **environ;
const char *__progname = "";

void _start1(fptr, int, char *[]) __dead2;

/* The entry function, C part. */
void
_start1(fptr cleanup, int argc, char *argv[])
{
	char **env;
	const char *s;
	size_t n, array_size;

	env = argv + argc + 1;
	environ = env;
	if (argc > 0 && argv[0] != NULL) {
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s + 1;
	}

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else
		_init_tls();

#ifdef GCRT
	atexit(_mcleanup);
#endif
	/*
 	 * The fini_array needs to be executed in the opposite order of its
	 * definition.  However, atexit works like a LIFO stack, so by
	 * pushing functions in array order, they will be executed in the
	 * reverse order as required.
	 */
	atexit(_fini);
	array_size = __fini_array_end - __fini_array_start;
	for (n = 0; n < array_size; n++)
		atexit(*__fini_array_start [n]);
#ifdef GCRT
	monstartup(&eprol, &etext);
__asm__("eprol:");
#endif
	if (&_DYNAMIC == NULL) {
		/*
		 * For static executables preinit happens right before init.
		 * Dynamic executable preinit arrays are handled by rtld
		 * before any DSOs are initialized.
		 */
		array_size = __preinit_array_end - __preinit_array_start;
		for (n = 0; n < array_size; n++)
			(*__preinit_array_start [n])(argc, argv, env);
	}
	_init();
	array_size = __init_array_end - __init_array_start;
	for (n = 0; n < array_size; n++)
		(*__init_array_start [n])(argc, argv, env);
	exit( main(argc, argv, env) );
}

__asm(".hidden	_start1");
