/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Rob Augustinus
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
 * AMD CPPC (Collaborative Processor Performance Control) cpufreq driver.
 *
 * This driver enables fine-grained CPU frequency scaling on AMD Zen 3+
 * processors using the CPPC2 MSR interface, replacing the legacy P-state
 * mechanism (hwpstate/Cool'n'Quiet) which only exposes a handful of discrete
 * frequency steps.
 *
 * CPPC allows the CPU to autonomously manage its frequency within bounds set
 * by the OS, guided by an Energy Performance Preference (EPP) hint. This
 * enables much lower idle frequencies and smoother scaling than legacy
 * P-states.
 *
 * Reference: AMD PPR for Family 19h, ACPI 6.5 spec section 8.4.7, Linux
 * drivers/cpufreq/amd-pstate.c
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include "cpufreq_if.h"

/*
 * AMD CPPC MSR definitions.
 */
#define MSR_AMD_CPPC_CAP1		0xC00102B0
#define MSR_AMD_CPPC_ENABLE		0xC00102B1
#define MSR_AMD_CPPC_REQ		0xC00102B3

/* CPPC_CAP1 fields (read-only) */
#define AMD_CPPC_LOWEST_PERF(x)		(((x) >> 0) & 0xFF)
#define AMD_CPPC_LOWNONLIN_PERF(x)	(((x) >> 8) & 0xFF)
#define AMD_CPPC_NOMINAL_PERF(x)	(((x) >> 16) & 0xFF)
#define AMD_CPPC_HIGHEST_PERF(x)	(((x) >> 24) & 0xFF)

/* CPPC_REQ fields (read-write) */
#define AMD_CPPC_MAX_PERF_SHIFT		0
#define AMD_CPPC_MIN_PERF_SHIFT		8
#define AMD_CPPC_DES_PERF_SHIFT		16
#define AMD_CPPC_EPP_PERF_SHIFT		24

#define AMD_CPPC_REQ_BUILD(max, min, des, epp)	\
	(((uint64_t)(epp) << AMD_CPPC_EPP_PERF_SHIFT) |	\
	 ((uint64_t)(des) << AMD_CPPC_DES_PERF_SHIFT) |	\
	 ((uint64_t)(min) << AMD_CPPC_MIN_PERF_SHIFT) |	\
	 ((uint64_t)(max) << AMD_CPPC_MAX_PERF_SHIFT))

/* CPPC_ENABLE */
#define AMD_CPPC_ENABLE_BIT		(1ULL << 0)

/* CPUID feature detection */
#define CPUID_AMD_EXT_FEATURES		0x80000008

/* Maximum frequency steps we expose to cpufreq */
#define AMD_CPPC_MAX_SETTINGS		64

extern uint64_t tsc_freq;

static int	amd_cppc_verbose = 0;
SYSCTL_INT(_debug, OID_AUTO, amd_cppc_verbose, CTLFLAG_RWTUN,
	   &amd_cppc_verbose, 0, "Debug AMD CPPC driver");

#define CPPC_DEBUG(dev, fmt, ...)					\
	do {								\
		if (amd_cppc_verbose)					\
			device_printf(dev, fmt, ## __VA_ARGS__);	\
	} while (0)

struct amd_cppc_softc {
	device_t	dev;
	int		cpu_id;

	/* Capabilities from CPPC_CAP1 */
	uint8_t		highest_perf;
	uint8_t		nominal_perf;
	uint8_t		lowest_nonlinear_perf;
	uint8_t		lowest_perf;

	/* Current request state */
	uint8_t		req_max_perf;
	uint8_t		req_min_perf;
	uint8_t		req_des_perf;
	uint8_t		req_epp;

	/* Frequency mapping */
	int		base_freq_mhz;	/* nominal frequency in MHz */

	/* EPP control */
	int		epp;	/* 0-100 user-facing scale */

	bool		cppc_enabled;
};

/*
 * Bind current thread to a specific CPU for MSR access.
 */
static void
amd_cppc_bind_cpu(int cpu_id)
{

	thread_lock(curthread);
	sched_bind(curthread, cpu_id);
	thread_unlock(curthread);
}

static void
amd_cppc_unbind_cpu(void)
{

	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);
}

/*
 * Read an MSR on the CPU associated with this softc.
 */
static uint64_t
amd_cppc_rdmsr(struct amd_cppc_softc *sc, uint32_t msr){
	uint64_t	val;

	amd_cppc_bind_cpu(sc->cpu_id);
	val = rdmsr(msr);
	amd_cppc_unbind_cpu();
	return (val);
}

/*
 * Write an MSR on the CPU associated with this softc.
 */
static void
amd_cppc_wrmsr(struct amd_cppc_softc *sc, uint32_t msr, uint64_t val)
{

	amd_cppc_bind_cpu(sc->cpu_id);
	wrmsr(msr, val);
	amd_cppc_unbind_cpu();
}

/*
 * Convert abstract performance level to MHz. Uses the relationship: freq =
 * base_freq * perf / nominal_perf
 */
static int
amd_cppc_perf_to_mhz(struct amd_cppc_softc *sc, uint8_t perf)
{

	if (sc->nominal_perf == 0)
		return (0);
	return ((int)((uint64_t) sc->base_freq_mhz * perf / sc->nominal_perf));
}

/*
 * Convert MHz to abstract performance level. Clamps to [lowest_perf,
 * highest_perf].
 */
static uint8_t
amd_cppc_mhz_to_perf(struct amd_cppc_softc *sc, int mhz){
	int		perf;

	if (sc->base_freq_mhz == 0)
		return (sc->nominal_perf);
	perf = (int)((uint64_t) mhz * sc->nominal_perf / sc->base_freq_mhz);
	if (perf < sc->lowest_perf)
		perf = sc->lowest_perf;
	if (perf > sc->highest_perf)
		perf = sc->highest_perf;
	return ((uint8_t) perf);
}

/*
 * Convert user-facing EPP (0-100) to hardware EPP (0-255). 0 = maximum
 * performance, 100 = maximum efficiency.
 */
static uint8_t
amd_cppc_epp_to_hw(int epp){

	if (epp < 0)
		epp = 0;
	if (epp > 100)
		epp = 100;
	return ((uint8_t) (epp * 255 / 100));
}

/*
 * Write the CPPC request register with current softc state.
 */
static void
amd_cppc_write_req(struct amd_cppc_softc *sc)
{
	uint64_t	val;

	val = AMD_CPPC_REQ_BUILD(sc->req_max_perf, sc->req_min_perf,
				 sc->req_des_perf, sc->req_epp);
	amd_cppc_wrmsr(sc, MSR_AMD_CPPC_REQ, val);
}

/*
 * Enable CPPC on the CPU associated with this softc.
 */
static int
amd_cppc_enable(struct amd_cppc_softc *sc)
{
	uint64_t	val;

	val = amd_cppc_rdmsr(sc, MSR_AMD_CPPC_ENABLE);
	if ((val & AMD_CPPC_ENABLE_BIT) == 0) {
		val |= AMD_CPPC_ENABLE_BIT;
		amd_cppc_wrmsr(sc, MSR_AMD_CPPC_ENABLE, val);

		/* Verify it took */
		val = amd_cppc_rdmsr(sc, MSR_AMD_CPPC_ENABLE);
		if ((val & AMD_CPPC_ENABLE_BIT) == 0) {
			device_printf(sc->dev,
			   "failed to enable CPPC on CPU %d\n", sc->cpu_id);
			return (ENXIO);
		}
	}
	sc->cppc_enabled = true;
	CPPC_DEBUG(sc->dev, "CPPC enabled on CPU %d\n", sc->cpu_id);
	return (0);
}

/*
 * Disable CPPC on the CPU associated with this softc.
 */
static void
amd_cppc_disable(struct amd_cppc_softc *sc)
{
	uint64_t	val;

	if (!sc->cppc_enabled)
		return;

	/* Zero out the request register first */
	amd_cppc_wrmsr(sc, MSR_AMD_CPPC_REQ, 0);

	val = amd_cppc_rdmsr(sc, MSR_AMD_CPPC_ENABLE);
	val &= ~AMD_CPPC_ENABLE_BIT;
	amd_cppc_wrmsr(sc, MSR_AMD_CPPC_ENABLE, val);
	sc->cppc_enabled = false;
	CPPC_DEBUG(sc->dev, "CPPC disabled on CPU %d\n", sc->cpu_id);
}

/*
 * Read CPPC capabilities from CAP1 MSR.
 */
static int
amd_cppc_read_caps(struct amd_cppc_softc *sc)
{
	uint64_t	cap1;

	cap1 = amd_cppc_rdmsr(sc, MSR_AMD_CPPC_CAP1);
	sc->highest_perf = AMD_CPPC_HIGHEST_PERF(cap1);
	sc->nominal_perf = AMD_CPPC_NOMINAL_PERF(cap1);
	sc->lowest_nonlinear_perf = AMD_CPPC_LOWNONLIN_PERF(cap1);
	sc->lowest_perf = AMD_CPPC_LOWEST_PERF(cap1);

	if (sc->highest_perf == 0 || sc->nominal_perf == 0 ||
	    sc->lowest_perf == 0) {
		device_printf(sc->dev, "invalid CPPC capabilities on CPU %d: "
			   "highest=%u nominal=%u lowest_nl=%u lowest=%u\n",
			      sc->cpu_id, sc->highest_perf, sc->nominal_perf,
			      sc->lowest_nonlinear_perf, sc->lowest_perf);
		return (ENXIO);
	}

	if (sc->lowest_perf > sc->nominal_perf ||
	    sc->nominal_perf > sc->highest_perf) {
		device_printf(sc->dev, "inconsistent CPPC capabilities on "
			      "CPU %d\n", sc->cpu_id);
		return (ENXIO);
	}

	return (0);
}

/*
 * Sysctl handler for EPP (Energy Performance Preference). User-facing range:
 * 0 (max performance) to 100 (max efficiency).
 */
static int
amd_cppc_sysctl_epp(SYSCTL_HANDLER_ARGS)
{
	struct amd_cppc_softc *sc;
	device_t	dev;
	int		epp, error;

	dev = (device_t) arg1;
	sc = device_get_softc(dev);
	epp = sc->epp;

	error = sysctl_handle_int(oidp, &epp, 0, req);
	if (error || req->newptr == NULL)
		return (error);

	if (epp < 0 || epp > 100)
		return (EINVAL);

	sc->epp = epp;
	sc->req_epp = amd_cppc_epp_to_hw(epp);

	if (sc->cppc_enabled)
		amd_cppc_write_req(sc);

	CPPC_DEBUG(dev, "EPP set to %d (hw: %u) on CPU %d\n",
		   epp, sc->req_epp, sc->cpu_id);
	return (0);
}

/*
 * Check if this CPU supports AMD CPPC via CPUID.
 */
static bool
amd_cppc_supported(void)
{
	u_int		regs[4];

	if (cpu_vendor_id != CPU_VENDOR_AMD)
		return (false);

	/* Need at least Family 17h (Zen) for CPPC MSRs */
	if (CPUID_TO_FAMILY(cpu_id) < 0x17)
		return (false);

	/* Check CPPC bit in extended features */
	do_cpuid(CPUID_AMD_EXT_FEATURES, regs);
	if ((regs[1] & AMDFEID_CPPC) == 0)
		return (false);

	return (true);
}

/*
 * Device methods.
 */
static void
amd_cppc_identify(driver_t * driver, device_t parent)
{

	if (!amd_cppc_supported())
		return;

	/* Only add if we haven't already */
	if (device_find_child(parent, "amd_cppc", -1) != NULL)
		return;

	if (BUS_ADD_CHILD(parent, 15, "amd_cppc",
			  device_get_unit(parent)) == NULL)
		device_printf(parent, "amd_cppc: add child failed\n");
}

static int
amd_cppc_probe(device_t dev)
{

	if (!amd_cppc_supported())
		return (ENXIO);

	if (resource_disabled("amd_cppc", 0))
		return (ENXIO);

	device_set_desc(dev, "AMD CPPC Frequency Control");
	return (BUS_PROBE_SPECIFIC);
}

static int
amd_cppc_attach(device_t dev)
{
	struct amd_cppc_softc *sc;
	int		error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->cpu_id = device_get_unit(device_get_parent(dev));

	/* Derive base frequency from TSC */
	sc->base_freq_mhz = (int)(tsc_freq / 1000000);
	if (sc->base_freq_mhz == 0) {
		device_printf(dev, "unable to determine base frequency\n");
		return (ENXIO);
	}

	/* Read capabilities */
	error = amd_cppc_read_caps(sc);
	if (error)
		return (error);

	device_printf(dev,
		      "CPU %d: highest=%u(%d MHz) nominal=%u(%d MHz) "
		      "lowest_nl=%u(%d MHz) lowest=%u(%d MHz)\n",
		      sc->cpu_id,
		      sc->highest_perf,
		      amd_cppc_perf_to_mhz(sc, sc->highest_perf),
		      sc->nominal_perf,
		      amd_cppc_perf_to_mhz(sc, sc->nominal_perf),
		      sc->lowest_nonlinear_perf,
		      amd_cppc_perf_to_mhz(sc, sc->lowest_nonlinear_perf),
		      sc->lowest_perf,
		      amd_cppc_perf_to_mhz(sc, sc->lowest_perf));

	/* Set default EPP to balanced */
	sc->epp = 50;
	sc->req_epp = amd_cppc_epp_to_hw(50);

	/* Default request: full range, autonomous mode */
	sc->req_max_perf = sc->highest_perf;
	sc->req_min_perf = sc->lowest_perf;
	sc->req_des_perf = 0;	/* 0 = autonomous, let CPU decide */

	/* Enable CPPC */
	error = amd_cppc_enable(sc);
	if (error)
		return (error);

	/* Write initial request */
	amd_cppc_write_req(sc);

	/* Create sysctl nodes */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		     SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
			"epp", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
			dev, 0, amd_cppc_sysctl_epp, "I",
			"Energy Performance Preference "
			"(0 = max performance, 100 = max efficiency)");

	SYSCTL_ADD_U8(device_get_sysctl_ctx(dev),
		      SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		      "highest_perf", CTLFLAG_RD, &sc->highest_perf, 0,
		      "Highest performance capability");

	SYSCTL_ADD_U8(device_get_sysctl_ctx(dev),
		      SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		      "nominal_perf", CTLFLAG_RD, &sc->nominal_perf, 0,
		      "Nominal (sustained) performance capability");

	SYSCTL_ADD_U8(device_get_sysctl_ctx(dev),
		      SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		      "lowest_perf", CTLFLAG_RD, &sc->lowest_perf, 0,
		      "Lowest performance capability");

	/* Register with cpufreq framework */
	return (cpufreq_register(dev));
}

static int
amd_cppc_detach(device_t dev)
{
	struct amd_cppc_softc *sc;

	sc = device_get_softc(dev);
	amd_cppc_disable(sc);
	return (cpufreq_unregister(dev));
}

static int
amd_cppc_suspend(device_t dev)
{
	struct amd_cppc_softc *sc;

	sc = device_get_softc(dev);
	amd_cppc_disable(sc);
	return (0);
}

static int
amd_cppc_resume(device_t dev)
{
	struct amd_cppc_softc *sc;
	int		error;

	sc = device_get_softc(dev);

	/* Re-read caps in case firmware changed anything */
	error = amd_cppc_read_caps(sc);
	if (error)
		return (error);

	/* Re-enable and restore request */
	error = amd_cppc_enable(sc);
	if (error)
		return (error);
	amd_cppc_write_req(sc);
	return (0);
}

/*
 * cpufreq driver interface methods.
 */

/*
 * Return available frequency settings.
 *
 * Generate evenly-spaced steps from highest to lowest performance. powerd
 * picks from these; when it calls set(), we translate back to a CPPC
 * performance level.
 */
static int
amd_cppc_settings(device_t dev, struct cf_setting *sets, int *count)
{
	struct amd_cppc_softc *sc;
	int		perf_range, step, n, perf;

	sc = device_get_softc(dev);
	if (!sc->cppc_enabled)
		return (ENXIO);

	perf_range = sc->highest_perf - sc->lowest_perf;
	if (perf_range <= 0) {
		*count = 0;
		return (0);
	}

	/*
	 * Calculate step size to produce a reasonable number of entries.
	 * Target ~30 steps, minimum step of 1.
	 */
	step = perf_range / 30;
	if (step < 1)
		step = 1;

	/* Generate entries from highest down to just above lowest */
	n = 0;
	for (perf = sc->highest_perf; perf > sc->lowest_perf;
	     perf -= step) {
		if (n >= *count || n >= AMD_CPPC_MAX_SETTINGS)
			break;
		memset(&sets[n], 0, sizeof(sets[n]));
		sets[n].freq = amd_cppc_perf_to_mhz(sc, (uint8_t) perf);
		sets[n].volts = CPUFREQ_VAL_UNKNOWN;
		sets[n].power = CPUFREQ_VAL_UNKNOWN;
		sets[n].lat = 1;	/* ~1 us transition latency */
		sets[n].dev = dev;
		n++;
	}

	/* Always include the lowest performance level */
	if (n < *count && n < AMD_CPPC_MAX_SETTINGS) {
		memset(&sets[n], 0, sizeof(sets[n]));
		sets[n].freq = amd_cppc_perf_to_mhz(sc, sc->lowest_perf);
		sets[n].volts = CPUFREQ_VAL_UNKNOWN;
		sets[n].power = CPUFREQ_VAL_UNKNOWN;
		sets[n].lat = 1;
		sets[n].dev = dev;
		n++;
	}

	*count = n;
	return (0);
}

/*
 * Set the target frequency.
 *
 * We interpret the target frequency as the maximum performance cap. The CPU
 * autonomously manages its actual frequency between lowest_perf and the cap,
 * guided by EPP.
 */
static int
amd_cppc_set(device_t dev, const struct cf_setting *cf)
{
	struct amd_cppc_softc *sc;
	uint8_t		target_perf;

	sc = device_get_softc(dev);
	if (!sc->cppc_enabled)
		return (ENXIO);

	target_perf = amd_cppc_mhz_to_perf(sc, cf->freq);

	sc->req_max_perf = target_perf;
	sc->req_min_perf = sc->lowest_perf;
	sc->req_des_perf = 0;	/* autonomous mode */

	amd_cppc_write_req(sc);

	CPPC_DEBUG(dev, "CPU %d: set max_perf=%u (%d MHz), epp=%u\n",
		   sc->cpu_id, target_perf, cf->freq, sc->req_epp);
	return (0);
}

/*
 * Get the current frequency setting.
 *
 * Returns the last-requested max performance as a frequency. For actual
 * measured frequency, users can check aperf/mperf.
 */
static int
amd_cppc_get(device_t dev, struct cf_setting *cf)
{
	struct amd_cppc_softc *sc;

	sc = device_get_softc(dev);
	if (!sc->cppc_enabled)
		return (ENXIO);

	memset(cf, 0, sizeof(*cf));
	cf->freq = amd_cppc_perf_to_mhz(sc, sc->req_max_perf);
	cf->volts = CPUFREQ_VAL_UNKNOWN;
	cf->power = CPUFREQ_VAL_UNKNOWN;
	cf->lat = CPUFREQ_VAL_UNKNOWN;
	cf->dev = dev;
	return (0);
}

static int
amd_cppc_type(device_t dev, int *type)
{

	if (type == NULL)
		return (EINVAL);
	*type = CPUFREQ_TYPE_ABSOLUTE | CPUFREQ_FLAG_UNCACHED;
	return (0);
}

static device_method_t amd_cppc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, amd_cppc_identify),
		DEVMETHOD(device_probe, amd_cppc_probe),
		DEVMETHOD(device_attach, amd_cppc_attach),
		DEVMETHOD(device_detach, amd_cppc_detach),
		DEVMETHOD(device_suspend, amd_cppc_suspend),
		DEVMETHOD(device_resume, amd_cppc_resume),

	/* cpufreq interface */
		DEVMETHOD(cpufreq_drv_set, amd_cppc_set),
		DEVMETHOD(cpufreq_drv_get, amd_cppc_get),
		DEVMETHOD(cpufreq_drv_type, amd_cppc_type),
		DEVMETHOD(cpufreq_drv_settings, amd_cppc_settings),

		DEVMETHOD_END
};

static driver_t amd_cppc_driver = {
	"amd_cppc",
		amd_cppc_methods,
		sizeof(struct amd_cppc_softc),
};

DRIVER_MODULE(amd_cppc, cpu, amd_cppc_driver, NULL, NULL);
MODULE_VERSION(amd_cppc, 1);
