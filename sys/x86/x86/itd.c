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
 * Intel Thread Director: per-logical-CPU enable state.
 *
 * Thread Director extends the Hardware Feedback Interface with a
 * classification of the running thread (IA32_THREAD_FEEDBACK_CHAR) and
 * per-class columns in the HFI table.  hfi(4) owns the package-level
 * enable bit and the table; this file owns the two per-CPU MSRs that go
 * with it, IA32_HW_FEEDBACK_THREAD_CONFIG (classification on/off) and
 * IA32_HRESET_ENABLE (which history the HRESET instruction may reset), and
 * the two flags the scheduler reads.  Everything here runs in thread
 * context via smp_rendezvous(); the scheduler side (sched_mic(4)) only
 * reads itd_hreset_mask and itd_nclasses and executes HRESET and one rdmsr.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <x86/itd.h>
#include <x86/x86_var.h>

u_int __read_mostly itd_hreset_mask;
u_int __read_mostly itd_nclasses;

/* Mask HRESET_ENABLE will take, from CPUID; 0 if HRESET is unusable. */
static u_int itd_hreset_avail;
static bool itd_enabled;

/*
 * Whether the boot CPU reports Thread Director with at least one class.
 * On hybrid parts the feature bits are the same on every core.
 */
bool
itd_supported(u_int *nclasses)
{
	u_int n;

	if (cpu_vendor_id != CPU_VENDOR_INTEL || cpu_high < 6)
		return (false);
	if ((cpu_power_eax & (CPUTPM1_HW_FEEDBACK | CPUTPM1_THREAD_DIRECTOR))
	    != (CPUTPM1_HW_FEEDBACK | CPUTPM1_THREAD_DIRECTOR))
		return (false);
	n = (cpu_power_ecx & CPUID_PERF_TD_CLASSES) >> 8;
	if (n == 0)
		return (false);
	if (nclasses != NULL)
		*nclasses = n;
	return (true);
}

/*
 * The Thread Director history bit of IA32_HRESET_ENABLE, if the
 * processor has HRESET (CPUID.(07H,1):EAX[22]) and lists that component
 * (CPUID.20H:EBX[0]).  Other components are never enabled here.
 */
static u_int
itd_hreset_probe(void)
{
	u_int regs[4];

	if (cpu_high < 0x20)
		return (0);
	cpuid_count(7, 1, regs);
	if ((regs[0] & CPUID_STDEXT4_HRESET) == 0)
		return (0);
	cpuid_count(0x20, 0, regs);
	return (regs[1] & CPUID_HRESET_THDIR);
}

/* smp_rendezvous actions; the mask is passed so it cannot change under us. */
static void
itd_cpu_enable(void *arg)
{
	u_int mask = *(u_int *)arg;

	wrmsr(MSR_IA32_HW_FEEDBACK_THREAD_CONFIG,
	    IA32_HW_FEEDBACK_THREAD_CONFIG_EN);
	if (mask != 0)
		wrmsr(MSR_IA32_HRESET_ENABLE, mask);
}

static void
itd_cpu_disable(void *arg __unused)
{

	if (itd_hreset_avail != 0)
		wrmsr(MSR_IA32_HRESET_ENABLE, 0);
	wrmsr(MSR_IA32_HW_FEEDBACK_THREAD_CONFIG, 0);
}

/*
 * Turn classification on for every CPU and publish the class count.
 * The caller (hfi(4)) then sets the package-level Thread Director bit.
 * Idempotent, so resume can call it again.  nclasses is what the caller
 * will publish per-class scores for; it must not exceed the CPUID count.
 */
int
itd_enable(u_int nclasses)
{
	u_int hwclasses, mask;

	if (!itd_supported(&hwclasses))
		return (ENXIO);
	if (nclasses == 0 || nclasses > hwclasses)
		return (EINVAL);
	mask = itd_hreset_probe();
	itd_hreset_avail = mask;
	smp_rendezvous(NULL, itd_cpu_enable, NULL, &mask);
	itd_enabled = true;
	atomic_store_int(&itd_nclasses, nclasses);
	/* Only after every CPU has the MSR set may anyone execute HRESET. */
	atomic_store_rel_int(&itd_hreset_mask, mask);
	return (0);
}

/*
 * Turn it off again (suspend).  The mask is cleared first and the
 * rendezvous that clears the MSRs runs with interrupts disabled on every
 * CPU, so no CPU can be between its test of the mask and its HRESET when
 * its MSR is cleared.
 */
void
itd_disable(void)
{

	if (!itd_enabled)
		return;
	atomic_store_rel_int(&itd_hreset_mask, 0);
	atomic_store_int(&itd_nclasses, 0);
	smp_rendezvous(NULL, itd_cpu_disable, NULL, NULL);
	itd_enabled = false;
}

static int
sysctl_hw_itd_supported(SYSCTL_HANDLER_ARGS)
{
	u_int n;
	int v;

	v = itd_supported(&n) ? 1 : 0;
	return (sysctl_handle_int(oidp, &v, 0, req));
}

static int
sysctl_hw_itd_hreset(SYSCTL_HANDLER_ARGS)
{
	int v;

	v = itd_hreset_probe() != 0 ? 1 : 0;
	return (sysctl_handle_int(oidp, &v, 0, req));
}

SYSCTL_NODE(_hw, OID_AUTO, itd, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Intel Thread Director");
SYSCTL_PROC(_hw_itd, OID_AUTO, supported, CTLTYPE_INT | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_hw_itd_supported, "I",
    "Processor supports Thread Director (CPUID.06H:EAX[23])");
SYSCTL_PROC(_hw_itd, OID_AUTO, hreset, CTLTYPE_INT | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_hw_itd_hreset, "I",
    "Processor supports HRESET of Thread Director history");
SYSCTL_UINT(_hw_itd, OID_AUTO, classes, CTLFLAG_RD, &itd_nclasses, 0,
    "Thread Director classes in use (0 = disabled)");
SYSCTL_UINT(_hw_itd, OID_AUTO, hreset_mask, CTLFLAG_RD, &itd_hreset_mask, 0,
    "IA32_HRESET_ENABLE value in effect (0 = HRESET not used)");
