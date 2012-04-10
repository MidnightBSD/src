/* $MidnightBSD: src/sys/cddl/compat/opensolaris/kern/opensolaris.c,v 1.1 2012/03/31 17:05:08 laffer1 Exp $ */
/*-
 * Copyright 2007 John Birrell <jb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cddl/compat/opensolaris/kern/opensolaris.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/misc.h>

cpu_core_t	cpu_core[MAXCPU];
kmutex_t	cpu_lock;
solaris_cpu_t	solaris_cpu[MAXCPU];

/*
 *  OpenSolaris subsystem initialisation.
 */
static void
opensolaris_load(void *dummy)
{
	int i;

	printf("This module (opensolaris) contains code covered by the\n");
	printf("Common Development and Distribution License (CDDL)\n");
	printf("see http://opensolaris.org/os/licensing/opensolaris_license/\n");

	/*
	 * "Enable" all CPUs even though they may not exist just so
	 * that the asserts work. On FreeBSD, if a CPU exists, it is
	 * enabled.
	 */
	for (i = 0; i < MAXCPU; i++) {
		solaris_cpu[i].cpuid = i;
		solaris_cpu[i].cpu_flags &= CPU_ENABLE;
	}

	mutex_init(&cpu_lock, "OpenSolaris CPU lock", MUTEX_DEFAULT, NULL);
}

SYSINIT(opensolaris_register, SI_SUB_OPENSOLARIS, SI_ORDER_FIRST, opensolaris_load, NULL);

static void
opensolaris_unload(void)
{
	mutex_destroy(&cpu_lock);
}

SYSUNINIT(opensolaris_unregister, SI_SUB_OPENSOLARIS, SI_ORDER_FIRST, opensolaris_unload, NULL);

static int
opensolaris_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

DEV_MODULE(opensolaris, opensolaris_modevent, NULL);
MODULE_VERSION(opensolaris, 1);
