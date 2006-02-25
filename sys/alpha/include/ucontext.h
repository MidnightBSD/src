/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD: src/sys/alpha/include/ucontext.h,v 1.7 2003/07/24 07:34:31 marcel Exp $
 */

#ifndef _MACHINE_UCONTEXT_H_
#define	_MACHINE_UCONTEXT_H_

typedef struct __mcontext {
	/*
	 * These fields must match the definition
	 * of struct sigcontext. That way we can support
	 * struct sigcontext and ucontext_t at the same
	 * time.
	 */
	long	mc_onstack;		/* XXX - sigcontext compat. */
	unsigned long mc_regs[37];
	unsigned long mc_fpregs[32];
	unsigned long mc_fpcr;
	unsigned long mc_fp_control;
#define	_MC_FPOWNED_NONE	0	/* FP state not used */
#define	_MC_FPOWNED_FPU		1	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		2	/* FP state came from PCB */
	long	mc_ownedfp;
#define	_MC_REV0_SIGFRAME	1	/* context is a signal frame */
#define	_MC_REV0_TRAPFRAME	2	/* context is a trap frame */
	long	mc_format;
	long	mc_thrptr;		/* Thread pointer */
	long	mc_spare[5];
} mcontext_t;

#if defined(_KERNEL) && defined(COMPAT_FREEBSD4)
struct mcontext4 {
	long	mc_onstack;		/* XXX - sigcontext compat. */
	unsigned long mc_regs[37];
	unsigned long mc_fpregs[32];
	unsigned long mc_fpcr;
	unsigned long mc_fp_control;
	long	mc_ownedfp;
	long	__spare__[7];
};
#endif

#endif /* !_MACHINE_UCONTEXT_H_ */
