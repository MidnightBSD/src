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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_execve.c,v 1.17 2007/01/12 07:25:25 imp Exp $
 */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(_execve, execve);

int 
_execve(const char *name, char *const * argv, char *const * envp)
{
	struct pthread	*curthread = _get_curthread();
	int		flags;
	int             i;
	int             ret;
	struct sigaction act;
	struct sigaction oact;
	struct itimerval itimer;

	/* Disable the interval timer: */
	itimer.it_interval.tv_sec  = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_sec     = 0;
	itimer.it_value.tv_usec    = 0;
	setitimer(_ITIMER_SCHED_TIMER, &itimer, NULL);

	/* Close the pthread kernel pipe: */
	__sys_close(_thread_kern_pipe[0]);
	__sys_close(_thread_kern_pipe[1]);

	/*
	 * Enter a loop to set all file descriptors to blocking
	 * if they were not created as non-blocking:
	 */
	for (i = 0; i < _thread_dtablesize; i++) {
		/* Check if this file descriptor is in use: */
		if (_thread_fd_table[i] != NULL &&
		    (_thread_fd_getflags(i) & O_NONBLOCK) == 0) {
			/* Skip if the close-on-exec flag is set */
			flags = __sys_fcntl(i, F_GETFD, NULL);
			if ((flags & FD_CLOEXEC) != 0)
				continue;	/* don't bother, no point */
			/* Get the current flags: */
			flags = __sys_fcntl(i, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			__sys_fcntl(i, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

	/* Enter a loop to adopt the signal actions for the running thread: */
	for (i = 1; i < NSIG; i++) {
		/* Check for signals which cannot be caught: */
		if (i == SIGKILL || i == SIGSTOP) {
			/* Don't do anything with these signals. */
		} else {
			/* Check if ignoring this signal: */
			if (_thread_sigact[i - 1].sa_handler == SIG_IGN) {
				/* Continue to ignore this signal: */
				act.sa_handler = SIG_IGN;
			} else {
				/* Use the default handler for this signal: */
				act.sa_handler = SIG_DFL;
			}

			/* Copy the mask and flags for this signal: */
			act.sa_mask = _thread_sigact[i - 1].sa_mask;
			act.sa_flags = _thread_sigact[i - 1].sa_flags;

			/* Change the signal action for the process: */
			__sys_sigaction(i, &act, &oact);
		}
	}

	/* Set the signal mask: */
	__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);

	/* Execute the process: */
	ret = __sys_execve(name, argv, envp);

	/* Return the completion status: */
	return (ret);
}
