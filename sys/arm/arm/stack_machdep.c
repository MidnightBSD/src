/*-
 * Copyright (c) 2005 Antoine Brodin
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/arm/arm/stack_machdep.c 250810 2013-05-19 16:25:09Z andrew $");

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/stack.h>

/*
 * This code makes assumptions about the stack layout. These are correct
 * when using APCS (the old ABI), but are no longer true with AAPCS and the
 * ARM EABI. There is also an issue with clang and llvm when building for
 * APCS where it lays out the stack incorrectly. Because of this we disable
 * this when building for ARM EABI or when building with clang.
 */
static void
stack_capture(struct stack *st, u_int32_t *frame)
{
#if !defined(__ARM_EABI__) && !defined(__clang__)
	vm_offset_t callpc;

	while (INKERNEL(frame)) {
		callpc = frame[FR_SCP];
		if (stack_put(st, callpc) == -1)
			break;
		frame = (u_int32_t *)(frame[FR_RFP]);
	}
#endif
}

void
stack_save_td(struct stack *st, struct thread *td)
{
	u_int32_t *frame;

	if (TD_IS_SWAPPED(td))
		panic("stack_save_td: swapped");
	if (TD_IS_RUNNING(td))
		panic("stack_save_td: running");

	/*
	 * This register, the frame pointer, is incorrect for the ARM EABI
	 * as it doesn't have a frame pointer, however it's value is not used
	 * when building for EABI.
	 */
	frame = (u_int32_t *)td->td_pcb->un_32.pcb32_r11;
	stack_zero(st);
	stack_capture(st, frame);
}

void
stack_save(struct stack *st)
{
	u_int32_t *frame;

	frame = (u_int32_t *)__builtin_frame_address(0);
	stack_zero(st);
	stack_capture(st, frame);
}
