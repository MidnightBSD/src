/*-
 * Copyright (c) 2011,2013 Justin Hibbits
 * Copyright (c) 2005, Joseph Koshy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/hwpmc/hwpmc_powerpc.c 255228 2013-09-05 01:13:26Z jhibbits $");

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/pmc_mdep.h>
#include <machine/spr.h>
#include <machine/pte.h>
#include <machine/sr.h>
#include <machine/cpu.h>
#include <machine/vmparam.h> /* For VM_MIN_KERNEL_ADDRESS/VM_MAX_KERNEL_ADDRESS */

#include "hwpmc_powerpc.h"

#define INKERNEL(x)	(((vm_offset_t)(x)) <= VM_MAX_KERNEL_ADDRESS && \
		((vm_offset_t)(x)) >= VM_MIN_KERNEL_ADDRESS)

struct powerpc_cpu **powerpc_pcpu;

int
pmc_save_kernel_callchain(uintptr_t *cc, int maxsamples,
    struct trapframe *tf)
{
	int frames = 0;
	uintptr_t *sp;

	cc[frames++] = tf->srr0;
	sp = (uintptr_t *)tf->fixreg[1];

	for (frames = 1; frames < maxsamples; frames++) {
		if (!INKERNEL(sp))
			break;
		cc[frames++] = *(sp + 1);
		sp = (uintptr_t *)*sp;
	}
	return (frames);
}

static int
powerpc_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{
	return (0);
}

static int
powerpc_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{
	return (0);
}

int
powerpc_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char powerpc_name[PMC_NAME_MAX];

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d], illegal CPU %d", __LINE__, cpu));

	phw = &powerpc_pcpu[cpu]->pc_ppcpmcs[ri];
	snprintf(powerpc_name, sizeof(powerpc_name), "POWERPC-%d", ri);
	if ((error = copystr(powerpc_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return error;
	pi->pm_class = PMC_CLASS_PPC7450;
	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc	       = NULL;
	}

	return (0);
}

int
powerpc_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = powerpc_pcpu[cpu]->pc_ppcpmcs[ri].phw_pmc;

	return (0);
}

struct pmc_mdep *
pmc_md_initialize()
{
	struct pmc_mdep *pmc_mdep;
	int error;
	uint16_t vers;
	
	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	powerpc_pcpu = malloc(sizeof(struct powerpc_cpu *) * pmc_cpu_max(), M_PMC,
			   M_WAITOK|M_ZERO);

	/* Just one class */
	pmc_mdep = pmc_mdep_alloc(1);

	pmc_mdep->pmd_cputype = PMC_CPU_PPC_7450;

	vers = mfpvr() >> 16;

	pmc_mdep->pmd_switch_in  = powerpc_switch_in;
	pmc_mdep->pmd_switch_out = powerpc_switch_out;
	
	switch (vers) {
	case MPC7447A:
	case MPC7448:
	case MPC7450:
	case MPC7455:
	case MPC7457:
		error = pmc_mpc7xxx_initialize(pmc_mdep);
	case IBM970:
	case IBM970FX:
	case IBM970MP:
	default:
		error = -1;
		break;
	}

	if (error != 0) {
		pmc_mdep_free(pmc_mdep);
		pmc_mdep = NULL;
		return NULL;
	}

	return (pmc_mdep);
}

void
pmc_md_finalize(struct pmc_mdep *md)
{
	free(md, M_PMC);
}

int
pmc_save_user_callchain(uintptr_t *cc, int maxsamples,
    struct trapframe *tf)
{
	(void) cc;
	(void) maxsamples;
	(void) tf;
	return (0);
}
