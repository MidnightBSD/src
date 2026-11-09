/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

/*
 * Intel Thread Director (ITD) per-thread classification support shared by
 * the hfi(4) driver, which enables it, and the SCHED_MIC scheduler, which
 * reads the class of the running thread and resets the classification
 * history when it switches threads.  See sys/x86/x86/itd.c.
 */

#ifndef _X86_ITD_H_
#define	_X86_ITD_H_

#ifdef _KERNEL

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

/*
 * Mask of history components HRESET may reset, as programmed into
 * IA32_HRESET_ENABLE on every CPU.  Non-zero only while itd_enable() has
 * programmed that MSR on every CPU, so a caller that tests it and then
 * executes HRESET with interrupts disabled cannot fault: the MSR is only
 * cleared again by a rendezvous that this CPU joins once it re-enables
 * interrupts.  Zero means HRESET must not be executed.
 */
extern u_int	itd_hreset_mask;
/* Number of Thread Director classes in use; 0 while ITD is disabled. */
extern u_int	itd_nclasses;

bool	itd_supported(u_int *nclasses);
int	itd_enable(u_int nclasses);
void	itd_disable(void);

/*
 * HRESET: reset the classification history of the current logical
 * processor for the components in EAX.
 *
 * ---- BEGIN AI-WRITTEN ASSEMBLY (phase F waiver, see PR) ----
 * Encoding F3 0F 3A F0 C0 ib is "HRESET imm8, <EAX>" (Intel SDM vol. 2A);
 * the ModRM byte C0 selects EAX and the immediate is ignored.  Written as
 * bytes because the assembler in use may not know the mnemonic.  The
 * instruction requires CPL 0 and raises #GP if any bit of EAX is clear in
 * IA32_HRESET_ENABLE, hence the mask discipline described above; with EAX
 * zero it is a NOP.  "memory" keeps it ordered with the thread switch.
 */
static __inline void
hreset(u_int mask)
{

	__asm __volatile(".byte 0xf3, 0x0f, 0x3a, 0xf0, 0xc0, 0x00"
	    : : "a" (mask) : "memory");
}
/* ---- END AI-WRITTEN ASSEMBLY ---- */

/*
 * Class of the thread running on this CPU, or -1 if the processor has no
 * valid classification yet.  One rdmsr; the caller checks itd_nclasses.
 */
static __inline int
itd_read_class(void)
{
	uint64_t v;

	v = rdmsr(MSR_IA32_THREAD_FEEDBACK_CHAR);
	if ((v & IA32_THREAD_FEEDBACK_CHAR_VALID) == 0)
		return (-1);
	return ((int)IA32_THREAD_FEEDBACK_CHAR_CLASS(v));
}

#endif /* _KERNEL */
#endif /* !_X86_ITD_H_ */
