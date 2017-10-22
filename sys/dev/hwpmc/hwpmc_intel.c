/*-
 * Copyright (c) 2008 Joseph Koshy
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

/*
 * Common code for handling Intel CPUs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

static int
intel_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{
	(void) pc;

	PMCDBG(MDP,SWI,1, "pc=%p pp=%p enable-msr=%d", pc, pp,
	    pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS);

	/* allow the RDPMC instruction if needed */
	if (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS)
		load_cr4(rcr4() | CR4_PCE);

	PMCDBG(MDP,SWI,1, "cr4=0x%jx", (uintmax_t) rcr4());

	return 0;
}

static int
intel_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{
	(void) pc;
	(void) pp;		/* can be NULL */

	PMCDBG(MDP,SWO,1, "pc=%p pp=%p cr4=0x%jx", pc, pp,
	    (uintmax_t) rcr4());

	/* always turn off the RDPMC instruction */
 	load_cr4(rcr4() & ~CR4_PCE);

	return 0;
}

struct pmc_mdep *
pmc_intel_initialize(void)
{
	struct pmc_mdep *pmc_mdep;
	enum pmc_cputype cputype;
	int error, model, nclasses, ncpus;

	KASSERT(cpu_vendor_id == CPU_VENDOR_INTEL,
	    ("[intel,%d] Initializing non-intel processor", __LINE__));

	PMCDBG(MDP,INI,0, "intel-initialize cpuid=0x%x", cpu_id);

	cputype = -1;
	nclasses = 2;

	model = ((cpu_id & 0xF0000) >> 12) | ((cpu_id & 0xF0) >> 4);

	switch (cpu_id & 0xF00) {
#if	defined(__i386__)
	case 0x500:		/* Pentium family processors */
		cputype = PMC_CPU_INTEL_P5;
		break;
#endif
	case 0x600:		/* Pentium Pro, Celeron, Pentium II & III */
		switch (model) {
#if	defined(__i386__)
		case 0x1:
			cputype = PMC_CPU_INTEL_P6;
			break;
		case 0x3: case 0x5:
			cputype = PMC_CPU_INTEL_PII;
			break;
		case 0x6: case 0x16:
			cputype = PMC_CPU_INTEL_CL;
			break;
		case 0x7: case 0x8: case 0xA: case 0xB:
			cputype = PMC_CPU_INTEL_PIII;
			break;
		case 0x9: case 0xD:
			cputype = PMC_CPU_INTEL_PM;
			break;
#endif
		case 0xE:
			cputype = PMC_CPU_INTEL_CORE;
			break;
		case 0xF:
			cputype = PMC_CPU_INTEL_CORE2;
			nclasses = 3;
			break;
		case 0x17:
			cputype = PMC_CPU_INTEL_CORE2EXTREME;
			nclasses = 3;
			break;
		case 0x1C:	/* Per Intel document 320047-002. */
			cputype = PMC_CPU_INTEL_ATOM;
			nclasses = 3;
			break;
		case 0x1A:
		case 0x1E:	/* Per Intel document 253669-032 9/2009, pages A-2 and A-57 */
		case 0x1F:	/* Per Intel document 253669-032 9/2009, pages A-2 and A-57 */
		case 0x2E:
			cputype = PMC_CPU_INTEL_COREI7;
			nclasses = 5;
			break;
		case 0x25:	/* Per Intel document 253669-033US 12/2009. */
		case 0x2C:	/* Per Intel document 253669-033US 12/2009. */
			cputype = PMC_CPU_INTEL_WESTMERE;
			nclasses = 5;
			break;
		case 0x2A:	/* Per Intel document 253669-039US 05/2011. */
			cputype = PMC_CPU_INTEL_SANDYBRIDGE;
			nclasses = 5;
			break;
		}
		break;
#if	defined(__i386__) || defined(__amd64__)
	case 0xF00:		/* P4 */
		if (model >= 0 && model <= 6) /* known models */
			cputype = PMC_CPU_INTEL_PIV;
		break;
	}
#endif

	if ((int) cputype == -1) {
		printf("pmc: Unknown Intel CPU.\n");
		return (NULL);
	}

	/* Allocate base class and initialize machine dependent struct */
	pmc_mdep = pmc_mdep_alloc(nclasses);

	pmc_mdep->pmd_cputype 	 = cputype;
	pmc_mdep->pmd_switch_in	 = intel_switch_in;
	pmc_mdep->pmd_switch_out = intel_switch_out;

	ncpus = pmc_cpu_max();

	error = pmc_tsc_initialize(pmc_mdep, ncpus);
	if (error)
		goto error;

	switch (cputype) {
#if	defined(__i386__) || defined(__amd64__)
		/*
		 * Intel Core, Core 2 and Atom processors.
		 */
	case PMC_CPU_INTEL_ATOM:
	case PMC_CPU_INTEL_CORE:
	case PMC_CPU_INTEL_CORE2:
	case PMC_CPU_INTEL_CORE2EXTREME:
	case PMC_CPU_INTEL_COREI7:
	case PMC_CPU_INTEL_SANDYBRIDGE:
	case PMC_CPU_INTEL_WESTMERE:
		error = pmc_core_initialize(pmc_mdep, ncpus);
		break;

		/*
		 * Intel Pentium 4 Processors, and P4/EMT64 processors.
		 */

	case PMC_CPU_INTEL_PIV:
		error = pmc_p4_initialize(pmc_mdep, ncpus);
		break;
#endif

#if	defined(__i386__)
		/*
		 * P6 Family Processors
		 */

	case PMC_CPU_INTEL_P6:
	case PMC_CPU_INTEL_CL:
	case PMC_CPU_INTEL_PII:
	case PMC_CPU_INTEL_PIII:
	case PMC_CPU_INTEL_PM:
		error = pmc_p6_initialize(pmc_mdep, ncpus);
		break;

		/*
		 * Intel Pentium PMCs.
		 */

	case PMC_CPU_INTEL_P5:
		error = pmc_p5_initialize(pmc_mdep, ncpus);
		break;
#endif

	default:
		KASSERT(0, ("[intel,%d] Unknown CPU type", __LINE__));
	}

	/*
	 * Init the uncore class.
	 */
#if	defined(__i386__) || defined(__amd64__)
	switch (cputype) {
		/*
		 * Intel Corei7 and Westmere processors.
		 */
	case PMC_CPU_INTEL_COREI7:
	case PMC_CPU_INTEL_SANDYBRIDGE:
	case PMC_CPU_INTEL_WESTMERE:
		error = pmc_uncore_initialize(pmc_mdep, ncpus);
		break;
	default:
		break;
	}
#endif

  error:
	if (error) {
		free(pmc_mdep, M_PMC);
		pmc_mdep = NULL;
	}

	return (pmc_mdep);
}

void
pmc_intel_finalize(struct pmc_mdep *md)
{
	pmc_tsc_finalize(md);

	switch (md->pmd_cputype) {
#if	defined(__i386__) || defined(__amd64__)
	case PMC_CPU_INTEL_ATOM:
	case PMC_CPU_INTEL_CORE:
	case PMC_CPU_INTEL_CORE2:
	case PMC_CPU_INTEL_CORE2EXTREME:
	case PMC_CPU_INTEL_COREI7:
	case PMC_CPU_INTEL_SANDYBRIDGE:
	case PMC_CPU_INTEL_WESTMERE:
		pmc_core_finalize(md);
		break;

	case PMC_CPU_INTEL_PIV:
		pmc_p4_finalize(md);
		break;
#endif
#if	defined(__i386__)
	case PMC_CPU_INTEL_P6:
	case PMC_CPU_INTEL_CL:
	case PMC_CPU_INTEL_PII:
	case PMC_CPU_INTEL_PIII:
	case PMC_CPU_INTEL_PM:
		pmc_p6_finalize(md);
		break;
	case PMC_CPU_INTEL_P5:
		pmc_p5_finalize(md);
		break;
#endif
	default:
		KASSERT(0, ("[intel,%d] unknown CPU type", __LINE__));
	}

	/*
	 * Uncore.
	 */
#if	defined(__i386__) || defined(__amd64__)
	switch (md->pmd_cputype) {
	case PMC_CPU_INTEL_COREI7:
	case PMC_CPU_INTEL_SANDYBRIDGE:
	case PMC_CPU_INTEL_WESTMERE:
		pmc_uncore_finalize(md);
		break;
	default:
		break;
	}
#endif
}
