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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_sigmask.c,v 1.10 2007/01/12 07:25:26 imp Exp $
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(_pthread_sigmask, pthread_sigmask);

int
_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t	sigset;
	int		ret = 0;

	/* Check if the existing signal process mask is to be returned: */
	if (oset != NULL) {
		/* Return the current mask: */
		*oset = curthread->sigmask;
	}
	/* Check if a new signal set was provided by the caller: */
	if (set != NULL) {
		/* Process according to what to do: */
		switch (how) {
		/* Block signals: */
		case SIG_BLOCK:
			/* Add signals to the existing mask: */
			SIGSETOR(curthread->sigmask, *set);
			break;

		/* Unblock signals: */
		case SIG_UNBLOCK:
			/* Clear signals from the existing mask: */
			SIGSETNAND(curthread->sigmask, *set);
			break;

		/* Set the signal process mask: */
		case SIG_SETMASK:
			/* Set the new mask: */
			curthread->sigmask = *set;
			break;

		/* Trap invalid actions: */
		default:
			/* Return an invalid argument: */
			errno = EINVAL;
			ret = -1;
			break;
		}

		/* Increment the sequence number: */
		curthread->sigmask_seqno++;

		/*
		 * Check if there are pending signals for the running
		 * thread or process that aren't blocked:
		 */
		sigset = curthread->sigpend;
		SIGSETOR(sigset, _process_sigpending);
		SIGSETNAND(sigset, curthread->sigmask);
		if (SIGNOTEMPTY(sigset))
			/*
			 * Call the kernel scheduler which will safely
			 * install a signal frame for the running thread:
			 */
			_thread_kern_sched_sig();
	}

	/* Return the completion status: */
	return (ret);
}
