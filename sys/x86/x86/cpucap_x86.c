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
 * x86 CPUID hybrid-core classification, as a static cpucap(9) provider.
 * Each present CPU probes its own CPUID (it must run on that CPU) to
 * determine its core type and the size of the last-level (L3) cache it
 * participates in; the provider's init routine then derives the per-CPU
 * class and a nominal capacity from those.
 *
 * Detection reliability:
 *   Intel P vs E   - architectural (CPUID 0x1a EAX[31:24]).
 *   Intel LP-E     - heuristic: SoC-tile LP-E cores have no L3, unlike
 *                    compute-tile E-cores; gated by kern.cpucap.detect_lpe.
 *   AMD X3D / Cx   - heuristic: the CCD/CCX with the larger L3 (3D V-Cache,
 *                    or the full-size cache of classic cores) is preferred;
 *                    symmetric or single-CCD parts stay all-perf (== ULE).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpucap.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

SYSCTL_DECL(_kern_cpucap);

/*
 * kern.cpucap.detect_lpe (loader tunable): classify Intel small cores
 * without an L3 as CPU_CLASS_LP instead of CPU_CLASS_EFF.
 * kern.sched.detect_lpe is accepted as a legacy alias.
 */
static int cpucap_x86_detect_lpe = 1;
SYSCTL_INT(_kern_cpucap, OID_AUTO, detect_lpe, CTLFLAG_RDTUN,
    &cpucap_x86_detect_lpe, 0,
    "Detect Intel LP-E cores (no L3) as class 4; 0 leaves them class 2");

static u_int	cpucap_x86_hybrid[MAXCPU];	/* CPUID 0x1a core type. */
static uint64_t	cpucap_x86_l3size[MAXCPU];	/* L3 bytes this CPU shares. */
static bool	cpucap_x86_has_l3[MAXCPU];	/* Whether an L3 was found. */

static bool
cpucap_x86_amd_x3d(void)
{

	return (strstr(cpu_model, "X3D") != NULL);
}

static uint64_t
cpucap_x86_cache_size(const u_int *regs)
{

	return ((uint64_t)CPUID_CACHE_WAYS(regs[1]) *
	    CPUID_CACHE_PARTITIONS(regs[1]) * CPUID_CACHE_LINE(regs[1]) *
	    CPUID_CACHE_SETS(regs[2]));
}

/* Runs on every CPU via smp_rendezvous(). */
static void
cpucap_x86_probe_cpu(void *arg __unused)
{
	u_int regs[4];
	u_int id;
	int i, leaf;

	id = PCPU_GET(cpuid);
	if (id >= MAXCPU)
		return;
	cpucap_x86_hybrid[id] = 0;
	cpucap_x86_l3size[id] = 0;
	cpucap_x86_has_l3[id] = false;

	if (cpu_vendor_id == CPU_VENDOR_INTEL && cpu_high >= 0x1a) {
		cpuid_count(0x1a, 0, regs);
		cpucap_x86_hybrid[id] = regs[0] & CPUID_HYBRID_CORE_MASK;
	}

	/* Deterministic cache enumeration: AMD leaf 0x8000001d, Intel 0x04. */
	if ((cpu_vendor_id == CPU_VENDOR_AMD ||
	    cpu_vendor_id == CPU_VENDOR_HYGON) && cpu_exthigh >= 0x8000001d)
		leaf = 0x8000001d;
	else if (cpu_vendor_id == CPU_VENDOR_INTEL && cpu_high >= 4)
		leaf = 0x04;
	else
		return;

	for (i = 0; ; i++) {
		cpuid_count(leaf, i, regs);
		if (CPUID_CACHE_TYPE(regs[0]) == CPUID_CACHE_TYPE_NULL)
			break;
		if (CPUID_CACHE_LEVEL(regs[0]) == 3) {
			cpucap_x86_l3size[id] = cpucap_x86_cache_size(regs);
			cpucap_x86_has_l3[id] = true;
			break;
		}
	}
}

/* Set a CPU's class and the nominal capacity that goes with it. */
static void
cpucap_x86_set(int cpu, uint8_t cls)
{
	uint16_t cap;

	switch (cls) {
	case CPU_CLASS_EFF:
		cap = CPUCAP_CAPACITY_EFF_NOMINAL;
		break;
	case CPU_CLASS_LP:
		cap = CPUCAP_CAPACITY_LP_NOMINAL;
		break;
	default:
		cap = CPUCAP_CAPACITY_PERF_NOMINAL;
		break;
	}
	cpucap_set_class(cpu, cls);
	cpucap_set_capacity(cpu, cap);
}

static int
cpucap_x86_probe(void)
{

	if (mp_ncpus <= 1)
		return (ENXIO);		/* Defaults: every CPU is class perf. */
	if (cpu_vendor_id != CPU_VENDOR_INTEL &&
	    cpu_vendor_id != CPU_VENDOR_AMD &&
	    cpu_vendor_id != CPU_VENDOR_HYGON)
		return (ENXIO);
	TUNABLE_INT_FETCH("kern.sched.detect_lpe", &cpucap_x86_detect_lpe);
	return (0);
}

static int
cpucap_x86_init(void)
{
	uint64_t maxl3;
	int i;
	bool asymmetric;

	/* Each CPU records its own type/cache; we then derive classes. */
	smp_rendezvous(NULL, cpucap_x86_probe_cpu, NULL, NULL);

	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		/* Compute cores are the P-cores; prefer them by default. */
		cpucap_set_compute_class(CPU_CLASS_PERF, true);
		CPU_FOREACH(i) {
			if (cpucap_x86_hybrid[i] == CPUID_HYBRID_LARGE_CORE)
				cpucap_x86_set(i, CPU_CLASS_PERF);
			else if (cpucap_x86_hybrid[i] ==
			    CPUID_HYBRID_SMALL_CORE)
				cpucap_x86_set(i, (cpucap_x86_detect_lpe != 0 &&
				    !cpucap_x86_has_l3[i]) ? CPU_CLASS_LP :
				    CPU_CLASS_EFF);
			/* Non-hybrid Intel keeps the default class perf. */
		}
		return (0);
	}

	/* AMD/Hygon: the die with the largest L3 is preferred (class perf). */
	maxl3 = 0;
	CPU_FOREACH(i)
		if (cpucap_x86_has_l3[i] && cpucap_x86_l3size[i] > maxl3)
			maxl3 = cpucap_x86_l3size[i];
	if (maxl3 == 0)
		return (0);		/* No L3 info; leave all class perf. */
	asymmetric = false;
	CPU_FOREACH(i)
		if (cpucap_x86_has_l3[i] && cpucap_x86_l3size[i] != maxl3) {
			asymmetric = true;
			break;
		}
	if (!asymmetric)
		return (0);		/* Single CCD / symmetric L3 == ULE. */
	/*
	 * The smaller-L3 CCD holds the compute (frequency) or C-cores.  Prefer
	 * it unless this is an X3D part, where the V-Cache CCD wins by default.
	 */
	cpucap_set_compute_class(CPU_CLASS_EFF, !cpucap_x86_amd_x3d());
	CPU_FOREACH(i)
		if (!(cpucap_x86_has_l3[i] && cpucap_x86_l3size[i] == maxl3))
			cpucap_x86_set(i, CPU_CLASS_EFF);
	return (0);
}

static struct cpucap_provider cpucap_x86_provider = {
	.cp_name = "cpuid",
	.cp_kind = CPUCAP_PROVIDER_STATIC,
	.cp_priority = 10,
	.cp_probe = cpucap_x86_probe,
	.cp_init = cpucap_x86_init,
};
CPUCAP_PROVIDER_DECLARE(x86, cpucap_x86_provider);
