/*-
 * Copyright (C) 1995-1997 Wolfgang Solfrank.
 * Copyright (C) 1995-1997 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: cpu.h,v 1.11 2000/05/26 21:19:53 thorpej Exp $
 * $FreeBSD: src/sys/powerpc/include/cpu.h,v 1.18 2005/01/07 02:29:19 imp Exp $
 */

#ifndef _MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>

#define	CLKF_USERMODE(frame)	(((frame)->srr1 & PSL_PR) != 0)
#define	CLKF_BASEPRI(frame)	((frame)->pri == 0)
#define	CLKF_PC(frame)		((frame)->srr0)
#define	CLKF_INTR(frame)	((frame)->depth > 0)

#define	TRAPF_USERMODE(frame)	(((frame)->srr1 & PSL_PR) != 0)
#define	TRAPF_PC(frame)		((frame)->srr0)

#define	cpu_swapout(p)
#define	cpu_number()		0

#if defined(_KERNEL) || defined(_STANDALONE)
#define	CACHELINESIZE	32
#endif

extern void __syncicache(void *, int);

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CACHELINE	1
#define CPU_CONSDEV	2
#define CPU_ADJKERNTZ   3
#define CPU_DISRTCSET   4
#define CPU_BOOTINFO    5 
#define CPU_WALLCLOCK   6
#define CPU_MAXID       7

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "cachelinesize", CTLTYPE_INT }, \
}

static __inline u_int64_t
get_cyclecount(void)
{
	u_int32_t _upper, _lower;
	u_int64_t _time;

	__asm __volatile(
		"mftb %0\n"
		"mftbu %1"
		: "=r" (_lower), "=r" (_upper));

	_time = (u_int64_t)_upper;
	_time = (_time << 32) + _lower;
	return (_time);
}

#define	cpu_getstack(td)	((td)->td_frame->fixreg[1])
#define	cpu_spinwait()		/* nothing */

void	cpu_halt(void);
void	cpu_reset(void);
void	fork_trampoline(void);
void	swi_vm(void *);

/* XXX the following should not be here. */
void	savectx(struct pcb *);
int	kcopy(const void *, void *, size_t);

#endif	/* _MACHINE_CPU_H_ */
