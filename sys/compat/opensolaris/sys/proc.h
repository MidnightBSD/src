/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/sys/compat/opensolaris/sys/proc.h 171863 2007-08-16 20:33:20Z pjd $
 */

#ifndef _OPENSOLARIS_SYS_PROC_H_
#define	_OPENSOLARIS_SYS_PROC_H_

#include <sys/param.h>
#include <sys/kthread.h>
#include_next <sys/proc.h>
#include <sys/stdint.h>
#include <sys/smp.h>
#include <sys/debug.h>

#ifdef _KERNEL

#define	CPU		curcpu
#define	minclsyspri	0
#define	maxclsyspri	0
#define	max_ncpus	mp_ncpus
#define	boot_max_ncpus	mp_ncpus

extern int hz;	/* system clock's frequency */

#define	TS_RUN	0

#define	p0	proc0

typedef	short		pri_t;
typedef	struct thread	_kthread;
typedef	struct thread	kthread_t;
typedef struct thread	*kthread_id_t;
typedef struct proc	proc_t;

#if (KSTACK_PAGES * PAGE_SIZE) < 16384
#define	ZFS_KSTACK_PAGES	(16384 / PAGE_SIZE)
#else
#define	ZFS_KSTACK_PAGES	0
#endif

static __inline kthread_t *
thread_create(caddr_t stk, size_t stksize, void (*proc)(void *), void *arg,
    size_t len, proc_t *pp, int state, pri_t pri)
{
	proc_t *p;
	int error;

	/*
	 * Be sure there are no surprises.
	 */
	ASSERT(stk == NULL);
	ASSERT(stksize == 0);
	ASSERT(len == 0);
	ASSERT(state == TS_RUN);

	error = kthread_create(proc, arg, &p, 0, ZFS_KSTACK_PAGES,
	    "solthread %p", proc);
	return (error == 0 ? FIRST_THREAD_IN_PROC(p) : NULL);
}

#define	thread_exit()	kthread_exit(0)

#endif	/* _KERNEL */

#endif	/* _OPENSOLARIS_SYS_PROC_H_ */
