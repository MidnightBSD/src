/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD
 * $FreeBSD: release/10.0.0/sys/mips/nlm/xlp.h 238290 2012-07-09 10:24:45Z jchandra $
 */

#ifndef __NLM_XLP_H__
#define __NLM_XLP_H__
#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/iomap.h>

#define	PIC_UART_0_IRQ	9

#define	PIC_PCIE_0_IRQ	11
#define	PIC_PCIE_1_IRQ	12
#define	PIC_PCIE_2_IRQ	13
#define	PIC_PCIE_3_IRQ	14

#define	PIC_EHCI_0_IRQ	16 
#define	PIC_MMC_IRQ	21
/* 41 used by IRQ_SMP */


/* XLP 8xx/4xx A0, A1, A2 CPU COP0 PRIDs */
#define	CHIP_PROCESSOR_ID_XLP_8XX		0x10
#define	CHIP_PROCESSOR_ID_XLP_3XX		0x11
#define	CHIP_PROCESSOR_ID_XLP_416		0x94
#define	CHIP_PROCESSOR_ID_XLP_432		0x14

/* Revision id's */
#define	XLP_REVISION_A0				0x00
#define	XLP_REVISION_A1				0x01
#define	XLP_REVISION_A2				0x02
#define	XLP_REVISION_B0				0x03
#define	XLP_REVISION_B1				0x04

#ifndef LOCORE
/*
 * FreeBSD can be started with few threads and cores turned off,
 * so have a hardware thread id to FreeBSD cpuid mapping.
 */
extern int xlp_ncores;
extern int xlp_threads_per_core;
extern uint32_t xlp_hw_thread_mask;
extern int xlp_cpuid_to_hwtid[];
extern int xlp_hwtid_to_cpuid[];
#ifdef SMP
extern void xlp_enable_threads(int code);
#endif
uint32_t xlp_get_cpu_frequency(int node, int core);
int nlm_set_device_frequency(int node, int devtype, int frequency);
int xlp_irt_to_irq(int irt);
int xlp_irq_to_irt(int irq);

static __inline int nlm_processor_id(void)
{
	return ((mips_rd_prid() >> 8) & 0xff);
}

static __inline int nlm_is_xlp3xx(void)
{

	return (nlm_processor_id() == CHIP_PROCESSOR_ID_XLP_3XX);
}

static __inline int nlm_is_xlp3xx_ax(void)
{
	uint32_t procid = mips_rd_prid();
	int prid = (procid >> 8) & 0xff;
	int rev = procid & 0xff;

	return (prid == CHIP_PROCESSOR_ID_XLP_3XX &&
		rev < XLP_REVISION_B0);
}

static __inline int nlm_is_xlp4xx(void)
{
	int prid = nlm_processor_id();

	return (prid == CHIP_PROCESSOR_ID_XLP_432 ||
	    prid == CHIP_PROCESSOR_ID_XLP_416);
}

static __inline int nlm_is_xlp8xx(void)
{
	int prid = nlm_processor_id();

	return (prid == CHIP_PROCESSOR_ID_XLP_8XX ||
	    prid == CHIP_PROCESSOR_ID_XLP_432 ||
	    prid == CHIP_PROCESSOR_ID_XLP_416);
}

static __inline int nlm_is_xlp8xx_ax(void)
{
	uint32_t procid = mips_rd_prid();
	int prid = (procid >> 8) & 0xff;
	int rev = procid & 0xff;

	return ((prid == CHIP_PROCESSOR_ID_XLP_8XX ||
	    prid == CHIP_PROCESSOR_ID_XLP_432 ||
	    prid == CHIP_PROCESSOR_ID_XLP_416) &&
	    (rev < XLP_REVISION_B0));
}

static __inline int nlm_is_xlp8xx_b0(void)
{
	uint32_t procid = mips_rd_prid();
	int prid = (procid >> 8) & 0xff;
	int rev = procid & 0xff;

	return ((prid == CHIP_PROCESSOR_ID_XLP_8XX ||
	    prid == CHIP_PROCESSOR_ID_XLP_432 ||
	    prid == CHIP_PROCESSOR_ID_XLP_416) &&
		rev == XLP_REVISION_B0);
}

#endif /* LOCORE */
#endif /* __NLM_XLP_H__ */
