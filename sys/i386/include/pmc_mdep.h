/*-
 * Copyright (c) 2003-2005 Joseph Koshy
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
 * $FreeBSD: release/7.0.0/sys/i386/include/pmc_mdep.h 147191 2005-06-09 19:45:09Z jkoshy $
 */

#ifndef _MACHINE_PMC_MDEP_H
#define	_MACHINE_PMC_MDEP_H 1

/*
 * On the i386 platform we support the following PMCs.
 *
 * K7		AMD Athlon XP/MP and other 32 bit processors.
 * K8		AMD Athlon64 and Opteron PMCs in 32 bit mode.
 * PIV		Intel P4/HTT and P4/EMT64
 * PPRO		Intel Pentium Pro, Pentium-II, Pentium-III, Celeron and
 *		Pentium-M processors
 * PENTIUM	Intel Pentium MMX.
 */

#include <dev/hwpmc/hwpmc_amd.h> /* K7 and K8 */
#include <dev/hwpmc/hwpmc_piv.h>
#include <dev/hwpmc/hwpmc_ppro.h>
#include <dev/hwpmc/hwpmc_pentium.h>

/*
 * Architecture specific extensions to <sys/pmc.h> structures.
 */

union pmc_md_op_pmcallocate  {
	struct pmc_md_amd_op_pmcallocate	pm_amd;
 	struct pmc_md_ppro_op_pmcallocate	pm_ppro;
	struct pmc_md_pentium_op_pmcallocate	pm_pentium;
	struct pmc_md_p4_op_pmcallocate		pm_p4;
	uint64_t				__pad[4];
};

/* Logging */
#define	PMCLOG_READADDR		PMCLOG_READ32
#define	PMCLOG_EMITADDR		PMCLOG_EMIT32

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
union pmc_md_pmc  {
	struct pmc_md_amd_pmc	pm_amd;
	struct pmc_md_ppro_pmc	pm_ppro;
	struct pmc_md_pentium_pmc pm_pentium;
	struct pmc_md_p4_pmc	pm_p4;
};

struct pmc;

/*
 * Prototypes
 */

void	pmc_x86_lapic_enable_pmc_interrupt(void);

#endif /* _KERNEL */
#endif /* _MACHINE_PMC_MDEP_H */
