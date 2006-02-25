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
 * $FreeBSD: src/lib/libthr/thread/thr_sig.c,v 1.13 2005/04/02 01:20:00 davidxu Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "thr_private.h"

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

static void
sigcancel_handler(int sig, siginfo_t *info, ucontext_t *ucp)
{
	struct pthread *curthread = _get_curthread();

	if (curthread->cancelflags & THR_CANCEL_AT_POINT)
		pthread_testcancel();
	if (curthread->flags & THR_FLAGS_NEED_SUSPEND) {
		__sys_sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, NULL);
		_thr_suspend_check(curthread);
	}
}

void
_thr_suspend_check(struct pthread *curthread)
{
	long cycle;

	/* Async suspend. */
	_thr_signal_block(curthread);
	THR_LOCK(curthread);
	if ((curthread->flags & (THR_FLAGS_NEED_SUSPEND | THR_FLAGS_SUSPENDED))
		== THR_FLAGS_NEED_SUSPEND) {
		curthread->flags |= THR_FLAGS_SUSPENDED;
		while (curthread->flags & THR_FLAGS_NEED_SUSPEND) {
			cycle = curthread->cycle;
			THR_UNLOCK(curthread);
			_thr_signal_unblock(curthread);
			_thr_umtx_wait(&curthread->cycle, cycle, NULL);
			_thr_signal_block(curthread);
			THR_LOCK(curthread);
		}
		curthread->flags &= ~THR_FLAGS_SUSPENDED;
	}
	THR_UNLOCK(curthread);
	_thr_signal_unblock(curthread);
}

void
_thr_signal_init(void)
{
	struct sigaction act;

	/* Install cancel handler. */
	SIGEMPTYSET(act.sa_mask);
	act.sa_flags = SA_SIGINFO | SA_RESTART;
	act.sa_sigaction = (__siginfohandler_t *)&sigcancel_handler;
	__sys_sigaction(SIGCANCEL, &act, NULL);
}

void
_thr_signal_deinit(void)
{
}

__weak_reference(_sigaction, sigaction);

int
_sigaction(int sig, const struct sigaction * act, struct sigaction * oact)
{
	/* Check if the signal number is out of range: */
	if (sig < 1 || sig > _SIG_MAXSIG || sig == SIGCANCEL) {
		/* Return an invalid argument: */
		errno = EINVAL;
		return (-1);
	}

	return __sys_sigaction(sig, act, oact);
}

__weak_reference(_sigprocmask, sigprocmask);

int
_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	const sigset_t *p = set;
	sigset_t newset;

	if (how != SIG_UNBLOCK) {
		if (set != NULL) {
			newset = *set;
			SIGDELSET(newset, SIGCANCEL);
			p = &newset;
		}
	}
	return (__sys_sigprocmask(how, p, oset));
}

__weak_reference(_pthread_sigmask, pthread_sigmask);

int
_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	if (_sigprocmask(how, set, oset))
		return (errno);
	return (0);
}

__weak_reference(_sigsuspend, sigsuspend);

int
_sigsuspend(const sigset_t * set)
{
	struct pthread *curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int oldcancel;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;

	oldcancel = _thr_cancel_enter(curthread);
	ret = __sys_sigsuspend(pset);
	_thr_cancel_leave(curthread, oldcancel);

	return (ret);
}

__weak_reference(__sigwait, sigwait);
__weak_reference(__sigtimedwait, sigtimedwait);
__weak_reference(__sigwaitinfo, sigwaitinfo);

int
__sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int oldcancel;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;
	oldcancel = _thr_cancel_enter(curthread);
	ret = __sys_sigtimedwait(pset, info, timeout);
	_thr_cancel_leave(curthread, oldcancel);
	return (ret);
}

int
__sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int oldcancel;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;

	oldcancel = _thr_cancel_enter(curthread);
	ret = __sys_sigwaitinfo(pset, info);
	_thr_cancel_leave(curthread, oldcancel);
	return (ret);
}

int
__sigwait(const sigset_t *set, int *sig)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int oldcancel;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else 
		pset = set;

	oldcancel = _thr_cancel_enter(curthread);
	ret = __sys_sigwait(pset, sig);
	_thr_cancel_leave(curthread, oldcancel);
	return (ret);
}
