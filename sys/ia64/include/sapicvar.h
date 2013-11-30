/*-
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD: src/sys/ia64/include/sapicvar.h,v 1.4 2003/09/10 22:49:38 marcel Exp $
 */

#ifndef _MACHINE_SAPICVAR_H_
#define _MACHINE_SAPICVAR_H_

struct sapic {
	int		sa_id;		/* I/O SAPIC Id */
	int		sa_base;	/* ACPI vector base */
	int		sa_limit;	/* last ACPI vector handled here */
	vm_offset_t	sa_registers;	/* virtual address of sapic */
};

#define SAPIC_TRIGGER_EDGE	0
#define SAPIC_TRIGGER_LEVEL	1

#define SAPIC_POLARITY_HIGH	0
#define SAPIC_POLARITY_LOW	1

#define SAPIC_DELMODE_FIXED	0
#define SAPIC_DELMODE_LOWPRI	1
#define SAPIC_DELMODE_PMI	2
#define SAPIC_DELMODE_NMI	4
#define SAPIC_DELMODE_INIT	5
#define SAPIC_DELMODE_EXTINT	7

int	sapic_config_intr(int irq, enum intr_trigger, enum intr_polarity);
struct sapic *sapic_create(int id, int base, uint64_t address);
int	sapic_enable(int irq, int vector);
void	sapic_eoi(struct sapic *sa, int vector);
#ifdef DDB
void	sapic_print(struct sapic *sa, int input);
#endif

#endif /* ! _MACHINE_SAPICVAR_H_ */
