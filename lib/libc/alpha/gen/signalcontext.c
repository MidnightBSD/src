/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/alpha/gen/signalcontext.c,v 1.1 2003/08/09 05:37:54 deischen Exp $");

#include <sys/param.h>
#include <sys/ucontext.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>

typedef void (*handler_t)(uint64_t, uint64_t, uint64_t);

/* Prototypes */
static void ctx_wrapper(ucontext_t *ucp, handler_t func, uint64_t *args);

__weak_reference(__signalcontext, signalcontext);

int
__signalcontext(ucontext_t *ucp, int sig, __sighandler_t *func)
{
	uint64_t *args;
	siginfo_t *sig_si;
	ucontext_t *sig_uc;
	uint64_t sp;

	/* Bail out if we don't have a valid ucontext pointer. */
	if (ucp == NULL)
		abort();

	/*
	 * Build a signal frame and copy the arguments of signal handler
	 * 'func' onto the stack. We only need 3 arguments, but we
	 * create room for 4 so that we are 16-byte aligned.
	 */
	sp = (ucp->uc_mcontext.mc_regs[FRAME_SP] - sizeof(ucontext_t)) & ~15UL;
	sig_uc = (ucontext_t *)sp;
	bcopy(ucp, sig_uc, sizeof(*sig_uc));
	sp = (sp - sizeof(siginfo_t)) & ~15UL;
	sig_si = (siginfo_t *)sp;
	bzero(sig_si, sizeof(*sig_si));
	sig_si->si_signo = sig;
	sp -= 4 * sizeof(uint64_t);
	args = (uint64_t *)sp;
	args[0] = sig;
	args[1] = (intptr_t)sig_si;
	args[2] = (intptr_t)sig_uc;
	args[3] = 0;

	/*
	 * Setup the ucontext of the signal handler.
	 */
	bzero(&ucp->uc_mcontext, sizeof(ucp->uc_mcontext));
	ucp->uc_link = sig_uc;
	sigdelset(&ucp->uc_sigmask, sig);

	ucp->uc_mcontext.mc_format = _MC_REV0_TRAPFRAME;
	ucp->uc_mcontext.mc_regs[FRAME_A0] = (register_t)ucp;
	ucp->uc_mcontext.mc_regs[FRAME_A1] = (register_t)func;
	ucp->uc_mcontext.mc_regs[FRAME_A1] = (register_t)args;
	ucp->uc_mcontext.mc_regs[FRAME_SP] = (register_t)sp;
	ucp->uc_mcontext.mc_regs[FRAME_PC] = (register_t)ctx_wrapper;
	ucp->uc_mcontext.mc_regs[FRAME_RA] = (register_t)ctx_wrapper;
	ucp->uc_mcontext.mc_regs[FRAME_T12] = (register_t)ctx_wrapper;
	return (0);
}

static void
ctx_wrapper(ucontext_t *ucp, handler_t func, uint64_t *args)
{

	(*func)(args[0], args[1], args[2]);
	if (ucp->uc_link == NULL)
		exit(0);
	setcontext((const ucontext_t *)ucp->uc_link);
	/* should never get here */
	abort();
	/* NOTREACHED */
}
