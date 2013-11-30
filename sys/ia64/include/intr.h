/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 * $FreeBSD: src/sys/ia64/include/intr.h,v 1.3 2003/01/05 22:14:30 marcel Exp $
 */

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

/*
 * Layout of the Processor Interrupt Block.
 */
struct ia64_interrupt_block
{
	u_int64_t	ib_ipi[0x20000];	/* 1Mb of IPI interrupts */
	u_int8_t	ib_reserved1[0xe0000];
	u_int8_t	ib_inta;		/* Generate INTA cycle */
	u_int8_t	ib_reserved2[7];
	u_int8_t	ib_xtp;			/* XTP cycle */
	u_int8_t	ib_reserved3[7];
	u_int8_t	ib_reserved4[0x1fff0];
};

extern u_int64_t ia64_lapic_address;

#define IA64_INTERRUPT_BLOCK	\
	(struct ia64_interrupt_block *)IA64_PHYS_TO_RR6(ia64_lapic_address)

struct sapic;

void ia64_add_sapic(struct sapic *sa);
int ia64_setup_intr(const char *name, int irq, driver_intr_t handler,
		    void *arg, enum intr_type flags, void **cookiep,
		    volatile long *cntp);
int ia64_teardown_intr(void *cookie);
void ia64_dispatch_intr(void *frame, unsigned long vector);

#endif /* !_MACHINE_INTR_H_ */
