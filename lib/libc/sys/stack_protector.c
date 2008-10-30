/* $FreeBSD: src/lib/libc/sys/stack_protector.c,v 1.2 2007/06/05 08:24:34 des Exp $ */
/* $NetBSD: stack_protector.c,v 1.4 2006/11/22 17:23:25 christos Exp $	*/
/* $OpenBSD: stack_protector.c,v 1.10 2006/03/31 05:34:44 deraadt Exp $	*/
/*
 * Copyright (c) 2002 Hiroaki Etoh, Federico G. Schwindt, and Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/sys/stack_protector.c,v 1.2 2007/06/05 08:24:34 des Exp $");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

extern int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen);

long __stack_chk_guard[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static void __guard_setup(void) __attribute__((__constructor__, __used__));
static void __fail(const char *);
void __stack_chk_fail(void);
void __chk_fail(void);
void __stack_chk_fail_local(void);

/*LINTED used*/
static void
__guard_setup(void)
{
	int mib[2];
	size_t len;

	if (__stack_chk_guard[0] != 0)
		return;

	mib[0] = CTL_KERN;
	mib[1] = KERN_ARND;

	len = sizeof(__stack_chk_guard);
	if (__sysctl(mib, 2, __stack_chk_guard, &len, NULL, 0) == -1 ||
	    len != sizeof(__stack_chk_guard)) {
		/* If sysctl was unsuccessful, use the "terminator canary". */
		((unsigned char *)(void *)__stack_chk_guard)[0] = 0;
		((unsigned char *)(void *)__stack_chk_guard)[1] = 0;
		((unsigned char *)(void *)__stack_chk_guard)[2] = '\n';
		((unsigned char *)(void *)__stack_chk_guard)[3] = 255;
	}
}

/*ARGSUSED*/
static void
__fail(const char *msg)
{
	struct sigaction sa;
	sigset_t mask;

	/* Immediately block all signal handlers from running code */
	(void)sigfillset(&mask);
	(void)sigdelset(&mask, SIGABRT);
	(void)sigprocmask(SIG_BLOCK, &mask, NULL);

	/* This may fail on a chroot jail... */
	syslog(LOG_CRIT, msg);

	(void)memset(&sa, 0, sizeof(sa));
	(void)sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_DFL;
	(void)sigaction(SIGABRT, &sa, NULL);
	(void)kill(getpid(), SIGABRT);
	_exit(127);
}

void
__stack_chk_fail(void)
{
	__fail("stack overflow detected; terminated");
}

void
__chk_fail(void)
{
	__fail("buffer overflow detected; terminated");
}

void
__stack_chk_fail_local(void)
{
	__stack_chk_fail();
}
