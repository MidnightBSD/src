/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/10.0.0/sys/mips/include/fdt.h 245332 2013-01-12 15:58:20Z rwatson $
 */

#ifndef _MACHINE_FDT_H_
#define _MACHINE_FDT_H_

#include <machine/bus.h>
#include <machine/intr_machdep.h>

/* Max interrupt number */
#if defined(CPU_RMI) || defined(CPU_NLM)
#define FDT_INTR_MAX	XLR_MAX_INTR
#else
#define FDT_INTR_MAX	(NHARD_IRQS + NSOFT_IRQS)
#endif

/* Map phandle/intpin pair to global IRQ number */ 
#define	FDT_MAP_IRQ(node, pin)	(pin)

/*
 * Bus space tag. XXX endianess info needs to be derived from the blob.
 */
#if defined(CPU_RMI) || defined(CPU_NLM)
#define fdtbus_bs_tag	rmi_uart_bus_space
#else
#define fdtbus_bs_tag	mips_bus_space_fdt
#endif

#endif /* _MACHINE_FDT_H_ */
