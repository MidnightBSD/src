/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 * $FreeBSD$
 */

#include "namespace.h"
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

__weak_reference(_pthread_once, pthread_once);

#define ONCE_NEVER_DONE		PTHREAD_NEEDS_INIT
#define ONCE_DONE		PTHREAD_DONE_INIT
#define	ONCE_IN_PROGRESS	0x02
#define	ONCE_MASK		0x03

static pthread_mutex_t		once_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t		once_cv = PTHREAD_COND_INITIALIZER;

/*
 * POSIX:
 * The pthread_once() function is not a cancellation point. However,
 * if init_routine is a cancellation point and is canceled, the effect
 * on once_control shall be as if pthread_once() was never called.
 */
 
static void
once_cancel_handler(void *arg)
{
	pthread_once_t *once_control = arg;

	_pthread_mutex_lock(&once_lock);
	once_control->state = ONCE_NEVER_DONE;
	_pthread_mutex_unlock(&once_lock);
	_pthread_cond_broadcast(&once_cv);
}

int
_pthread_once(pthread_once_t *once_control, void (*init_routine) (void))
{
	struct pthread *curthread;
	int wakeup = 0;

	if (once_control->state == ONCE_DONE)
		return (0);
	_pthread_mutex_lock(&once_lock);
	while (*(volatile int *)&(once_control->state) == ONCE_IN_PROGRESS)
		_pthread_cond_wait(&once_cv, &once_lock);
	/*
	 * If previous thread was canceled, then the state still
	 * could be ONCE_NEVER_DONE, we need to check it again.
	 */
	if (*(volatile int *)&(once_control->state) == ONCE_NEVER_DONE) {
		once_control->state = ONCE_IN_PROGRESS;
		_pthread_mutex_unlock(&once_lock);
		curthread = _get_curthread();
		THR_CLEANUP_PUSH(curthread, once_cancel_handler, once_control);
		init_routine();
		THR_CLEANUP_POP(curthread, 0);
		_pthread_mutex_lock(&once_lock);
		once_control->state = ONCE_DONE;
		wakeup = 1;
	}
	_pthread_mutex_unlock(&once_lock);
	if (wakeup)
		_pthread_cond_broadcast(&once_cv);
	return (0);
}

