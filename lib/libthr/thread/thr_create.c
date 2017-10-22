/*
 * Copyright (c) 2003 Daniel M. Eischen <deischen@gdeb.com>
 * Copyright (c) 2005, David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD: release/7.0.0/lib/libthr/thread/thr_create.c 165241 2006-12-15 11:52:01Z davidxu $
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/rtprio.h>
#include <sys/signalvar.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

static int  create_stack(struct pthread_attr *pattr);
static void thread_start(struct pthread *curthread);

__weak_reference(_pthread_create, pthread_create);

int
_pthread_create(pthread_t * thread, const pthread_attr_t * attr,
	       void *(*start_routine) (void *), void *arg)
{
	struct pthread *curthread, *new_thread;
	struct thr_param param;
	struct sched_param sched_param;
	struct rtprio rtp;
	int ret = 0, locked, create_suspended;
	sigset_t set, oset;

	_thr_check_init();

	/*
	 * Tell libc and others now they need lock to protect their data.
	 */
	if (_thr_isthreaded() == 0 && _thr_setthreaded(1))
		return (EAGAIN);

	curthread = _get_curthread();
	if ((new_thread = _thr_alloc(curthread)) == NULL)
		return (EAGAIN);

	memset(&param, 0, sizeof(param));

	if (attr == NULL || *attr == NULL)
		/* Use the default thread attributes: */
		new_thread->attr = _pthread_attr_default;
	else
		new_thread->attr = *(*attr);
	if (new_thread->attr.sched_inherit == PTHREAD_INHERIT_SCHED) {
		/* inherit scheduling contention scope */
		if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
			new_thread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
		else
			new_thread->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;

		new_thread->attr.prio = curthread->attr.prio;
		new_thread->attr.sched_policy = curthread->attr.sched_policy;
	}

	new_thread->tid = TID_TERMINATED;

	if (create_stack(&new_thread->attr) != 0) {
		/* Insufficient memory to create a stack: */
		_thr_free(curthread, new_thread);
		return (EAGAIN);
	}
	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	new_thread->magic = THR_MAGIC;
	new_thread->start_routine = start_routine;
	new_thread->arg = arg;
	new_thread->cancel_enable = 1;
	new_thread->cancel_async = 0;
	/* Initialize the mutex queue: */
	TAILQ_INIT(&new_thread->mutexq);
	TAILQ_INIT(&new_thread->pp_mutexq);

	/* Initialise hooks in the thread structure: */
	if (new_thread->attr.suspend == THR_CREATE_SUSPENDED) {
		new_thread->flags = THR_FLAGS_NEED_SUSPEND;
		create_suspended = 1;
	} else {
		create_suspended = 0;
	}

	new_thread->state = PS_RUNNING;

	if (new_thread->attr.flags & PTHREAD_CREATE_DETACHED)
		new_thread->tlflags |= TLFLAGS_DETACHED;

	/* Add the new thread. */
	new_thread->refcount = 1;
	_thr_link(curthread, new_thread);
	/* Return thread pointer eariler so that new thread can use it. */
	(*thread) = new_thread;
	if (SHOULD_REPORT_EVENT(curthread, TD_CREATE)) {
		THR_THREAD_LOCK(curthread, new_thread);
		locked = 1;
	} else
		locked = 0;
	param.start_func = (void (*)(void *)) thread_start;
	param.arg = new_thread;
	param.stack_base = new_thread->attr.stackaddr_attr;
	param.stack_size = new_thread->attr.stacksize_attr;
	param.tls_base = (char *)new_thread->tcb;
	param.tls_size = sizeof(struct tcb);
	param.child_tid = &new_thread->tid;
	param.parent_tid = &new_thread->tid;
	param.flags = 0;
	if (new_thread->attr.flags & PTHREAD_SCOPE_SYSTEM)
		param.flags |= THR_SYSTEM_SCOPE;
	if (new_thread->attr.sched_inherit == PTHREAD_INHERIT_SCHED)
		param.rtp = NULL;
	else {
		sched_param.sched_priority = new_thread->attr.prio;
		_schedparam_to_rtp(new_thread->attr.sched_policy,
			&sched_param, &rtp);
		param.rtp = &rtp;
	}

	/* Schedule the new thread. */
	if (create_suspended) {
		SIGFILLSET(set);
		SIGDELSET(set, SIGTRAP);
		__sys_sigprocmask(SIG_SETMASK, &set, &oset);
		new_thread->sigmask = oset;
	}

	ret = thr_new(&param, sizeof(param));

	if (ret != 0) {
		ret = errno;
		/*
		 * Translate EPROCLIM into well-known POSIX code EAGAIN.
		 */
		if (ret == EPROCLIM)
			ret = EAGAIN;
	}

	if (create_suspended)
		__sys_sigprocmask(SIG_SETMASK, &oset, NULL);

	if (ret != 0) {
		if (!locked)
			THR_THREAD_LOCK(curthread, new_thread);
		new_thread->state = PS_DEAD;
		new_thread->tid = TID_TERMINATED;
		if (new_thread->flags & THR_FLAGS_NEED_SUSPEND) {
			new_thread->cycle++;
			_thr_umtx_wake(&new_thread->cycle, INT_MAX);
		}
		THR_THREAD_UNLOCK(curthread, new_thread);
		THREAD_LIST_LOCK(curthread);
		_thread_active_threads--;
		new_thread->tlflags |= TLFLAGS_DETACHED;
		_thr_ref_delete_unlocked(curthread, new_thread);
		THREAD_LIST_UNLOCK(curthread);
		(*thread) = 0;
	} else if (locked) {
		_thr_report_creation(curthread, new_thread);
		THR_THREAD_UNLOCK(curthread, new_thread);
	}
	return (ret);
}

static int
create_stack(struct pthread_attr *pattr)
{
	int ret;

	/* Check if a stack was specified in the thread attributes: */
	if ((pattr->stackaddr_attr) != NULL) {
		pattr->guardsize_attr = 0;
		pattr->flags |= THR_STACK_USER;
		ret = 0;
	}
	else
		ret = _thr_stack_alloc(pattr);
	return (ret);
}

static void
thread_start(struct pthread *curthread)
{
	if (curthread->attr.suspend == THR_CREATE_SUSPENDED) {
		sigset_t set = curthread->sigmask;

		_thr_ast(curthread);

		/*
		 * Parent thread have stored signal mask for us,
		 * we should restore it now.
		 */
		sigprocmask(SIG_SETMASK, &set, NULL);
	}

	/*
	 * This is used as a serialization point to allow parent
	 * to report 'new thread' event to debugger before the thread
	 * does real work.
	 */
	THR_LOCK(curthread);
	THR_UNLOCK(curthread);

	/* Run the current thread's start routine with argument: */
	_pthread_exit(curthread->start_routine(curthread->arg));

	/* This point should never be reached. */
	PANIC("Thread has resumed after exit");
}
