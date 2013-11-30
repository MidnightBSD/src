/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)signal.h	8.3 (Berkeley) 3/30/94
 * $FreeBSD: src/include/signal.h,v 1.24 2003/03/31 23:30:41 jeff Exp $
 */

#ifndef _SIGNAL_H_
#define	_SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/_types.h>
#include <sys/signal.h>

#if __BSD_VISIBLE
/*
 * XXX should enlarge these, if only to give empty names instead of bounds
 * errors for large signal numbers.
 */
extern __const char *__const sys_signame[NSIG];
extern __const char *__const sys_siglist[NSIG];
extern __const int sys_nsig;
#endif

#if __POSIX_VISIBLE >= 200112 || __XSI_VISIBLE
#ifndef _PID_T_DECLARED
typedef	__pid_t		pid_t;
#define	_PID_T_DECLARED
#endif
#endif

__BEGIN_DECLS
int	raise(int);

#if __POSIX_VISIBLE || __XSI_VISIBLE
int	kill(__pid_t, int);
int	sigaction(int, const struct sigaction * __restrict,
	    struct sigaction * __restrict);
int	sigaddset(sigset_t *, int);
int	sigdelset(sigset_t *, int);
int	sigemptyset(sigset_t *);
int	sigfillset(sigset_t *);
int	sigismember(const sigset_t *, int);
int	sigpending(sigset_t *);
int	sigprocmask(int, const sigset_t * __restrict, sigset_t * __restrict);
int	sigsuspend(const sigset_t *);
int	sigwait(const sigset_t * __restrict, int * __restrict);
#endif

#if __POSIX_VISIBLE >= 199506 || __XSI_VISIBLE >= 600
#if 0
/*
 * PR: 35924
 * XXX we don't actually have these.  We set _POSIX_REALTIME_SIGNALS to
 * -1 to show that we don't have them, but this symbol is not necessarily
 * in scope (in the current implementation), so we can't use it here.
 */
int	sigqueue(__pid_t, int, const union sigval);
#endif
struct timespec;
int	sigtimedwait(const sigset_t * __restrict, siginfo_t * __restrict,
	    const struct timespec * __restrict);
int	sigwaitinfo(const sigset_t * __restrict, siginfo_t * __restrict);
#endif

#if __XSI_VISIBLE
int	killpg(__pid_t, int);
int	sigaltstack(const stack_t * __restrict, stack_t * __restrict); 
int	sigpause(int);
#endif

#if __POSIX_VISIBLE >= 200112
int	siginterrupt(int, int);
#endif

#if __BSD_VISIBLE
int	sigblock(int);
struct __ucontext;		/* XXX spec requires a complete declaration. */
int	sigreturn(const struct __ucontext *);
int	sigsetmask(int);
int	sigstack(const struct sigstack *, struct sigstack *);
int	sigvec(int, struct sigvec *, struct sigvec *);
void	psignal(unsigned int, const char *);
#endif
__END_DECLS

#endif /* !_SIGNAL_H_ */
