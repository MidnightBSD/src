/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Andrew Turner
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/systm.h>

#include <machine/cpu.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

extern bool pmap_multiple_tlbi;

typedef void (cpu_quirk_install)(void);
struct cpu_quirks {
	cpu_quirk_install *quirk_install;
	u_int		midr_mask;
	u_int		midr_value;
#define	CPU_QUIRK_POST_DEVICE	(1 << 0)	/* After device attach */
						/* e.g. needs SMCCC */
	u_int		flags;
};

static enum {
	SSBD_FORCE_ON,
	SSBD_FORCE_OFF,
	SSBD_KERNEL,
} ssbd_method = SSBD_KERNEL;

static cpu_quirk_install install_psci_bp_hardening;
static cpu_quirk_install install_ssbd_workaround;
static cpu_quirk_install install_thunderx_bcast_tlbi_workaround;
static cpu_quirk_install install_multiple_tlbi_workaround;

static struct cpu_quirks cpu_quirks[] = {
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A57,0,0),
		.quirk_install = install_psci_bp_hardening,
		.flags = CPU_QUIRK_POST_DEVICE,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A72,0,0),
		.quirk_install = install_psci_bp_hardening,
		.flags = CPU_QUIRK_POST_DEVICE,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A73,0,0),
		.quirk_install = install_psci_bp_hardening,
		.flags = CPU_QUIRK_POST_DEVICE,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A75,0,0),
		.quirk_install = install_psci_bp_hardening,
		.flags = CPU_QUIRK_POST_DEVICE,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDERX2, 0,0),
		.quirk_install = install_psci_bp_hardening,
		.flags = CPU_QUIRK_POST_DEVICE,
	},
	{
		.midr_mask = 0,
		.midr_value = 0,
		.quirk_install = install_ssbd_workaround,
		.flags = CPU_QUIRK_POST_DEVICE,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDERX, 0, 0),
		.quirk_install = install_thunderx_bcast_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDERX_81XX, 0, 0),
		.quirk_install = install_thunderx_bcast_tlbi_workaround,
	},
	/*
	 * ARM CPU errata: TLBI+DSB ordering may not observe preceding stores.
	 * Affects: C1-Premium 4193780, C1-Ultra 4193780, Cortex-A76 4193800,
	 * Cortex-A76AE 4193801, Cortex-A77 4193798, Cortex-A78 4193791,
	 * Cortex-A78AE 4193793, Cortex-A78C 4193794, Cortex-A710 4193788,
	 * Cortex-X1 4193791, Cortex-X1C 4193792, Cortex-X2 4193788,
	 * Cortex-X3 4193786, Cortex-X4 4118414, Cortex-X925 4193781,
	 * Neoverse-N1 4193800, Neoverse-N2 4193789, Neoverse-V1 4193790,
	 * Neoverse-V2 4193787, Neoverse-V3 4193784, Neoverse-V3AE 4193784.
	 * CVE-2025-10263 / FreeBSD-SA-26:31.arm64
	 */
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_C1_PREMIUM, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_C1_ULTRA, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A76, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A76AE, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A77, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A78, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A78AE, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A78C, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_A710, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_X1, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_X1C, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_X2, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_X3, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value = CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_X4, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_CORTEX_X925, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_NEOVERSE_N1, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_NEOVERSE_N2, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_NEOVERSE_V1, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_NEOVERSE_V2, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_NEOVERSE_V3, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_ARM, CPU_PART_NEOVERSE_V3AE, 0, 0),
		.quirk_install = install_multiple_tlbi_workaround,
	},
};

static void
install_psci_bp_hardening(void)
{
	/* SMCCC depends on PSCI. If PSCI is missing so is SMCCC */
	if (!psci_present)
		return;

	if (smccc_arch_features(SMCCC_ARCH_WORKAROUND_1) != SMCCC_RET_SUCCESS)
		return;

	PCPU_SET(bp_harden, smccc_arch_workaround_1);
}

static void
install_ssbd_workaround(void)
{
	char *env;

	if (PCPU_GET(cpuid) == 0) {
		env = kern_getenv("kern.cfg.ssbd");
		if (env != NULL) {
			if (strcmp(env, "force-on") == 0) {
				ssbd_method = SSBD_FORCE_ON;
			} else if (strcmp(env, "force-off") == 0) {
				ssbd_method = SSBD_FORCE_OFF;
			}
		}
	}

	/* SMCCC depends on PSCI. If PSCI is missing so is SMCCC */
	if (!psci_present)
		return;

	/* Enable the workaround on this CPU if it's enabled in the firmware */
	if (smccc_arch_features(SMCCC_ARCH_WORKAROUND_2) != SMCCC_RET_SUCCESS)
		return;

	switch(ssbd_method) {
	case SSBD_FORCE_ON:
		smccc_arch_workaround_2(1);
		break;
	case SSBD_FORCE_OFF:
		smccc_arch_workaround_2(0);
		break;
	case SSBD_KERNEL:
	default:
		PCPU_SET(ssbd, smccc_arch_workaround_2);
		break;
	}
}

/*
 * Workaround Cavium erratum 27456.
 *
 * Invalidate the local icache when changing address spaces.
 */
static void
install_thunderx_bcast_tlbi_workaround(void)
{
	u_int midr;

	midr = get_midr();
	if (CPU_PART(midr) == CPU_PART_THUNDERX_81XX)
		PCPU_SET(bcast_tlbi_workaround, 1);
	else if (CPU_PART(midr) == CPU_PART_THUNDERX) {
		if (CPU_VAR(midr) == 0) {
			/* ThunderX 1.x */
			PCPU_SET(bcast_tlbi_workaround, 1);
		} else if (CPU_VAR(midr) == 1 && CPU_REV(midr) <= 1) {
			/* ThunderX 2.0 - 2.1 */
			PCPU_SET(bcast_tlbi_workaround, 1);
		}
	}
}

static void
install_multiple_tlbi_workaround(void)
{
	pmap_multiple_tlbi = true;
}

static void
install_cpu_errata_flags(u_int mask, u_int flags)
{
	u_int midr;
	size_t i;

	midr = get_midr();

	for (i = 0; i < nitems(cpu_quirks); i++) {
		if ((midr & cpu_quirks[i].midr_mask) ==
		    cpu_quirks[i].midr_value &&
		    (cpu_quirks[i].flags & mask) == flags) {
			cpu_quirks[i].quirk_install();
		}
	}
}

/*
 * Install any CPU errata we need. On CPU 0 we only install the errata that
 * don't depend on device drivers as this is called early in the boot process.
 * On other CPUs the device drivers have already attached so install all
 * applicable errata.
 */
void
install_cpu_errata(void)
{
	/*
	 * Only install early CPU errata on CPU 0, device drivers may not
	 * have attached and some workarounds depend on them, e.g. to query
	 * SMCCC.
	 */
	if (PCPU_GET(cpuid) == 0) {
		install_cpu_errata_flags(CPU_QUIRK_POST_DEVICE, 0);
	} else {
		install_cpu_errata_flags(0, 0);
	}
}

/*
 * Install any errata workarounds that depend on device drivers, e.g. use
 * SMCCC to install a workaround.
 */
static void
install_cpu_errata_late(void *dummy __unused)
{
	MPASS(PCPU_GET(cpuid) == 0);
	install_cpu_errata_flags(CPU_QUIRK_POST_DEVICE, CPU_QUIRK_POST_DEVICE);
}
SYSINIT(install_cpu_errata_late, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE,
    install_cpu_errata_late, NULL);
