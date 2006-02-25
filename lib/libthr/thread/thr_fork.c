/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $FreeBSD: src/lib/libthr/thread/thr_fork.c,v 1.1 2005/04/02 01:20:00 davidxu Exp $
 */

/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <spinlock.h>

#include "libc_private.h"
#include "thr_private.h"

__weak_reference(_pthread_atfork, pthread_atfork);

int
_pthread_atfork(void (*prepare)(void), void (*parent)(void),
    void (*child)(void))
{
	struct pthread *curthread;
	struct pthread_atfork *af;

	_thr_check_init();

	if ((af = malloc(sizeof(struct pthread_atfork))) == NULL)
		return (ENOMEM);

	curthread = _get_curthread();
	af->prepare = prepare;
	af->parent = parent;
	af->child = child;
	THR_UMTX_LOCK(curthread, &_thr_atfork_lock);
	TAILQ_INSERT_TAIL(&_thr_atfork_list, af, qe);
	THR_UMTX_UNLOCK(curthread, &_thr_atfork_lock);
	return (0);
}

/*
 * For a while, allow libpthread to work with a libc that doesn't
 * export the malloc lock.
 */
#pragma weak __malloc_lock

__weak_reference(_fork, fork);

pid_t
_fork(void)
{
	static umtx_t inprogress;
	static int waiters;
	umtx_t tmp;

	struct pthread *curthread;
	struct pthread_atfork *af;
	pid_t ret;
	int errsave;
	int unlock_malloc;

	if (!_thr_is_inited())
		return (__sys_fork());

	curthread = _get_curthread();

	/*
	 * Block all signals until we reach a safe point.
	 */
	_thr_signal_block(curthread);

	THR_UMTX_LOCK(curthread, &_thr_atfork_lock);
	tmp = inprogress;
	while (tmp) {
		waiters++;
		THR_UMTX_UNLOCK(curthread, &_thr_atfork_lock);
		_thr_umtx_wait(&inprogress, tmp, NULL);
		THR_UMTX_LOCK(curthread, &_thr_atfork_lock);
		waiters--;
		tmp = inprogress;
	}
	inprogress = 1;

	/* Unlock mutex, allow new hook to be added during executing hooks. */
	THR_UMTX_UNLOCK(curthread, &_thr_atfork_lock);

	/* Run down atfork prepare handlers. */
	TAILQ_FOREACH_REVERSE(af, &_thr_atfork_list, atfork_head, qe) {
		if (af->prepare != NULL)
			af->prepare();
	}

	/*
	 * Try our best to protect memory from being corrupted in
	 * child process because another thread in malloc code will
	 * simply be kill by fork().
	 */
	if ((_thr_isthreaded() != 0) && (__malloc_lock != NULL)) {
		unlock_malloc = 1;
		_spinlock(__malloc_lock);
	} else {
		unlock_malloc = 0;
	}

	/* Fork a new process: */
	if ((ret = __sys_fork()) == 0) {
		/* Child process */
		errsave = errno;
		inprogress = 0;
		curthread->cancelflags &= ~THR_CANCEL_NEEDED;
		/*
		 * Thread list will be reinitialized, and later we call
		 * _libpthread_init(), it will add us back to list.
		 */
		curthread->tlflags &= ~(TLFLAGS_IN_TDLIST | TLFLAGS_DETACHED);

		/* child is a new kernel thread. */
		thr_self(&curthread->tid);

		/* clear other threads locked us. */
		_thr_umtx_init(&curthread->lock);
		_thr_umtx_init(&_thr_atfork_lock);
		_thr_setthreaded(0);

		/* reinitialize libc spinlocks, this includes __malloc_lock. */
		_thr_spinlock_init();
		_mutex_fork(curthread);

		/* reinitalize library. */
		_libpthread_init(curthread);

		/* Ready to continue, unblock signals. */ 
		_thr_signal_unblock(curthread);

		/* Run down atfork child handlers. */
		TAILQ_FOREACH(af, &_thr_atfork_list, qe) {
			if (af->child != NULL)
				af->child();
		}
	} else {
		/* Parent process */
		errsave = errno;

		if (unlock_malloc)
			_spinunlock(__malloc_lock);

		/* Ready to continue, unblock signals. */ 
		_thr_signal_unblock(curthread);

		/* Run down atfork parent handlers. */
		TAILQ_FOREACH(af, &_thr_atfork_list, qe) {
			if (af->parent != NULL)
				af->parent();
		}

		THR_UMTX_LOCK(curthread, &_thr_atfork_lock);
		inprogress = 0;
		if (waiters)
			_thr_umtx_wake(&inprogress, waiters);
		THR_UMTX_UNLOCK(curthread, &_thr_atfork_lock);
	}
	errno = errsave;

	/* Return the process ID: */
	return (ret);
}
