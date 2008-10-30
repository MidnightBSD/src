/*
 * Copyright (c) 1997 John Birrell <jb@cimlogic.com.au>.
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
 * 3. Neither the name of the author nor the names of any co-contributors
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_spinlock.c,v 1.13 2007/01/12 07:25:27 imp Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>

#include <libc_private.h>

#include "pthread_private.h"

void
_spinunlock(spinlock_t *lck)
{
	lck->access_lock = 0;
}

/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 */
void
_spinlock(spinlock_t *lck)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Try to grab the lock and loop if another thread grabs
	 * it before we do.
	 */
	while(_atomic_lock(&lck->access_lock)) {
		/* Block the thread until the lock. */
		curthread->data.spinlock = lck;
		_thread_kern_sched_state(PS_SPINBLOCK, __FILE__, __LINE__);
	}

	/* The running thread now owns the lock: */
	lck->lock_owner = (long) curthread;
}

/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 *
 * This function checks if the running thread has already locked the
 * location, warns if this occurs and creates a thread dump before
 * returning.
 */
void
_spinlock_debug(spinlock_t *lck, char *fname, int lineno)
{
	struct pthread	*curthread = _get_curthread();
	int cnt = 0;

	/*
	 * Try to grab the lock and loop if another thread grabs
	 * it before we do.
	 */
	while(_atomic_lock(&lck->access_lock)) {
		cnt++;
		if (cnt > 100) {
			char str[256];
			snprintf(str, sizeof(str), "%s - Warning: Thread %p attempted to lock %p from %s (%d) was left locked from %s (%d)\n", getprogname(), curthread, lck, fname, lineno, lck->fname, lck->lineno);
			__sys_write(2,str,strlen(str));
			__sleep(1);
			cnt = 0;
		}

		/* Block the thread until the lock. */
		curthread->data.spinlock = lck;
		_thread_kern_sched_state(PS_SPINBLOCK, fname, lineno);
	}

	/* The running thread now owns the lock: */
	lck->lock_owner = (long) curthread;
	lck->fname = fname;
	lck->lineno = lineno;
}
