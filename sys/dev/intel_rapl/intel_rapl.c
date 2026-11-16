/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Rafal Rabczuk
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Intel RAPL (Running Average Power Limit) package power limits and package
 * power measurement.
 *
 * The RAPL registers are per package, so one device attaches to the first
 * CPU of each package and every MSR access for that package is made on that
 * CPU.  Sysctl handlers bind the calling thread to it; the energy sampler is
 * a direct-execution callout, which runs from the clock interrupt of the CPU
 * it is scheduled on.
 *
 * Only the package domain is handled.  Silvermont and Airmont Atom parts
 * use different power, energy and time window encodings and are not
 * supported.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/intel_rapl/intel_rapl_calc.h>

/* Lowest limit accepted when the hardware does not report a minimum. */
#define	RAPL_DEFAULT_MIN_MW	5000

/* Longest window accepted when the hardware does not report a maximum. */
#define	RAPL_DEFAULT_MAX_WINDOW_US	(128ULL * 1000000)

/* Shift meaning "topology unknown, treat every CPU as one package". */
#define	RAPL_PKG_SHIFT_NONE	32

struct rapl_softc {
	device_t	sc_dev;
	struct sysctl_ctx_list sc_sysctl_ctx;
	u_int		sc_cpu;		/* CPU used for every MSR access */
	u_int		sc_pkg;

	u_int		sc_power_shift;
	u_int		sc_energy_shift;
	u_int		sc_time_shift;

	uint32_t	sc_tdp_mw;	/* 0 when not reported */
	uint32_t	sc_hw_min_mw;
	uint32_t	sc_hw_max_mw;
	uint64_t	sc_max_window_us;

	/* Protected by sc_sx. */
	struct sx	sc_sx;		/* serialises limit register changes */
	uint32_t	sc_min_allowed_mw;
	uint32_t	sc_max_allowed_mw;
	int		sc_locked;

	/* Protected by sc_mtx, a spin lock shared with the sampler. */
	struct mtx	sc_mtx;
	struct callout	sc_callout;
	bool		sc_detaching;
	uint32_t	sc_last_raw;
	sbintime_t	sc_last_sbt;
	uint64_t	sc_energy_ticks;
	uint32_t	sc_power_mw;
};

#define	RAPL_PL1	0
#define	RAPL_PL2	1

static void	rapl_identify(driver_t *driver, device_t parent);
static int	rapl_probe(device_t dev);
static int	rapl_attach(device_t dev);
static int	rapl_detach(device_t dev);

static device_method_t rapl_methods[] = {
	DEVMETHOD(device_identify,	rapl_identify),
	DEVMETHOD(device_probe,		rapl_probe),
	DEVMETHOD(device_attach,	rapl_attach),
	DEVMETHOD(device_detach,	rapl_detach),

	DEVMETHOD_END
};

static driver_t rapl_driver = {
	"intel_rapl",
	rapl_methods,
	sizeof(struct rapl_softc),
};

static devclass_t rapl_devclass;
DRIVER_MODULE(intel_rapl, cpu, rapl_driver, rapl_devclass, NULL, NULL);
MODULE_VERSION(intel_rapl, 1);

static bool
rapl_cpu_supported(void)
{

	if (cpu_vendor_id != CPU_VENDOR_INTEL)
		return (false);
	/* RAPL first appeared in Sandy Bridge, which has CPUID leaf 0Bh. */
	if (CPUID_TO_FAMILY(cpu_id) < 6 || cpu_high < 0x0b)
		return (false);
	if (CPUID_TO_FAMILY(cpu_id) == 6) {
		switch (CPUID_TO_MODEL(cpu_id)) {
		case 0x37:	/* Silvermont (Bay Trail) */
		case 0x4a:	/* Silvermont (Merrifield) */
		case 0x4c:	/* Airmont (Cherry Trail, Braswell) */
		case 0x5a:	/* Silvermont (Moorefield) */
		case 0x5d:	/* Silvermont (SoFIA) */
			return (false);
		}
	}
	return (true);
}

/*
 * Number of low APIC ID bits that identify a CPU within its package, from
 * CPUID leaf 1Fh or 0Bh.  Leaf 1Fh may report module, tile and die levels
 * above the core level, so use the shift of the last level reported.
 */
static u_int
rapl_pkg_shift(void)
{
	u_int leaf, p[4], shift;
	int i;

	p[1] = 0;
	leaf = 0;
	if (cpu_high >= 0x1f) {
		leaf = 0x1f;
		cpuid_count(leaf, 0, p);
	}
	if (p[1] == 0) {
		leaf = 0x0b;
		cpuid_count(leaf, 0, p);
	}
	if (p[1] == 0)
		return (RAPL_PKG_SHIFT_NONE);

	shift = RAPL_PKG_SHIFT_NONE;
	for (i = 0; i < 256; i++) {
		cpuid_count(leaf, i, p);
		if (((p[2] >> 8) & 0xff) == 0)
			break;
		shift = p[0] & 0x1f;
	}
	return (shift);
}

static u_int
rapl_pkg_id(struct pcpu *pc, u_int shift)
{

	if (shift >= RAPL_PKG_SHIFT_NONE)
		return (0);
	return (pc->pc_apic_id >> shift);
}

static void
rapl_identify(driver_t *driver, device_t parent)
{
	struct pcpu *pc;
	u_int pkg, shift;
	int i, unit;

	if (device_find_child(parent, "intel_rapl", -1) != NULL)
		return;
	if (!rapl_cpu_supported())
		return;

	/*
	 * cpu_get_pcpu() cannot be used on the cpu device itself: it asks the
	 * cpu device's parent (acpi0 or legacy0), which does not provide the
	 * ivar.  Both cpu drivers number their devices by CPU ID instead.
	 */
	unit = device_get_unit(parent);
	if (unit < 0 || unit > (int)mp_maxid || CPU_ABSENT(unit))
		return;
	pc = pcpu_find(unit);
	if (pc == NULL)
		return;

	/* Attach only to the lowest numbered CPU of each package. */
	shift = rapl_pkg_shift();
	pkg = rapl_pkg_id(pc, shift);
	CPU_FOREACH(i) {
		if (i >= pc->pc_cpuid)
			break;
		if (rapl_pkg_id(pcpu_find(i), shift) == pkg)
			return;
	}

	if (device_add_child(parent, "intel_rapl", -1) == NULL)
		device_printf(parent, "add intel_rapl child failed\n");
}

static int
rapl_probe(device_t dev)
{

	if (resource_disabled("intel_rapl", 0))
		return (ENXIO);
	device_set_desc(dev, "Intel RAPL package power limits");
	return (BUS_PROBE_GENERIC);
}

/*
 * Run the calling thread on the package's CPU, remembering how it was
 * bound before.  Only for contexts that may sleep.
 */
static void
rapl_bind(struct rapl_softc *sc, int *oldcpu, bool *wasbound)
{
	struct thread *td;

	td = curthread;
	thread_lock(td);
	*wasbound = sched_is_bound(td);
	*oldcpu = td->td_oncpu;
	sched_bind(td, sc->sc_cpu);
	thread_unlock(td);
}

static void
rapl_unbind(int oldcpu, bool wasbound)
{
	struct thread *td;

	td = curthread;
	thread_lock(td);
	if (wasbound)
		sched_bind(td, oldcpu);
	else
		sched_unbind(td);
	thread_unlock(td);
}

static int
rapl_read_limit(struct rapl_softc *sc, uint64_t *reg)
{
	bool wasbound;
	int error, oldcpu;

	rapl_bind(sc, &oldcpu, &wasbound);
	error = rdmsr_safe(MSR_PKG_POWER_LIMIT, reg);
	rapl_unbind(oldcpu, wasbound);
	return (error != 0 ? EIO : 0);
}

/*
 * Replace the bits in mask with bits, leaving the rest of the register
 * alone, then read it back.  The firmware may drop the write without
 * setting the lock bit; if the requested bits did not stick, put the old
 * value back and fail.  Called with sc_sx held.
 */
static int
rapl_modify_limit(struct rapl_softc *sc, uint64_t mask, uint64_t bits)
{
	uint64_t check, newreg, reg;
	bool wasbound;
	int error, oldcpu;

	sx_assert(&sc->sc_sx, SA_XLOCKED);

	rapl_bind(sc, &oldcpu, &wasbound);
	if (rdmsr_safe(MSR_PKG_POWER_LIMIT, &reg) != 0) {
		error = EIO;
		goto out;
	}
	if ((reg & RAPL_PL_LOCKED) != 0) {
		sc->sc_locked = 1;
		error = EPERM;
		goto out;
	}
	newreg = (reg & ~mask) | bits;
	if (newreg == reg) {
		error = 0;
		goto out;
	}
	if (wrmsr_safe(MSR_PKG_POWER_LIMIT, newreg) != 0) {
		error = EIO;
		goto out;
	}
	if (rdmsr_safe(MSR_PKG_POWER_LIMIT, &check) != 0) {
		error = EIO;
		goto out;
	}
	if ((check & mask) != bits) {
		(void)wrmsr_safe(MSR_PKG_POWER_LIMIT, reg);
		error = EPERM;
		goto out;
	}
	error = 0;
out:
	rapl_unbind(oldcpu, wasbound);
	return (error);
}

static int
rapl_sysctl_limit(SYSCTL_HANDLER_ARGS)
{
	struct rapl_softc *sc;
	uint64_t reg;
	uint32_t mw, units;
	u_int shift;
	int error;

	sc = arg1;
	shift = arg2 == RAPL_PL2 ? RAPL_PL2_SHIFT : 0;

	sx_xlock(&sc->sc_sx);
	error = rapl_read_limit(sc, &reg);
	if (error != 0)
		goto out;
	units = (uint32_t)((reg >> shift) & RAPL_PL_POWER_MASK);
	mw = (uint32_t)rapl_units_to_mw(units, sc->sc_power_shift);
	error = sysctl_handle_32(oidp, &mw, 0, req);
	if (error != 0 || req->newptr == NULL)
		goto out;

	if (mw < sc->sc_min_allowed_mw ||
	    (sc->sc_max_allowed_mw != 0 && mw > sc->sc_max_allowed_mw)) {
		error = EINVAL;
		goto out;
	}
	error = rapl_mw_to_units(mw, sc->sc_power_shift, &units);
	if (error != 0)
		goto out;
	/* A limit nobody enforces is not a limit: enable it as well. */
	error = rapl_modify_limit(sc, (RAPL_PL_POWER_MASK | RAPL_PL_ENABLE) <<
	    shift, ((uint64_t)units | RAPL_PL_ENABLE) << shift);
out:
	sx_xunlock(&sc->sc_sx);
	return (error);
}

static int
rapl_sysctl_enable(SYSCTL_HANDLER_ARGS)
{
	struct rapl_softc *sc;
	uint64_t reg;
	u_int shift;
	int error, val;

	sc = arg1;
	shift = arg2 == RAPL_PL2 ? RAPL_PL2_SHIFT : 0;

	sx_xlock(&sc->sc_sx);
	error = rapl_read_limit(sc, &reg);
	if (error != 0)
		goto out;
	val = (reg & (RAPL_PL_ENABLE << shift)) != 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		goto out;
	if (val != 0 && val != 1) {
		error = EINVAL;
		goto out;
	}
	error = rapl_modify_limit(sc, RAPL_PL_ENABLE << shift,
	    val != 0 ? RAPL_PL_ENABLE << shift : 0);
out:
	sx_xunlock(&sc->sc_sx);
	return (error);
}

static int
rapl_sysctl_window(SYSCTL_HANDLER_ARGS)
{
	struct rapl_softc *sc;
	uint64_t max, reg, us;
	uint32_t field;
	u_int shift;
	int error;

	sc = arg1;
	shift = (arg2 == RAPL_PL2 ? RAPL_PL2_SHIFT : 0) + RAPL_PL_WINDOW_SHIFT;

	sx_xlock(&sc->sc_sx);
	error = rapl_read_limit(sc, &reg);
	if (error != 0)
		goto out;
	us = rapl_window_to_us((uint32_t)((reg >> shift) & RAPL_PL_WINDOW_MASK),
	    sc->sc_time_shift);
	error = sysctl_handle_64(oidp, &us, 0, req);
	if (error != 0 || req->newptr == NULL)
		goto out;

	error = rapl_us_to_window(us, sc->sc_time_shift, &field);
	if (error != 0)
		goto out;
	max = sc->sc_max_window_us != 0 ? sc->sc_max_window_us :
	    RAPL_DEFAULT_MAX_WINDOW_US;
	if (rapl_window_to_us(field, sc->sc_time_shift) > max) {
		error = EINVAL;
		goto out;
	}
	error = rapl_modify_limit(sc, RAPL_PL_WINDOW_MASK << shift,
	    (uint64_t)field << shift);
out:
	sx_xunlock(&sc->sc_sx);
	return (error);
}

/* arg2 selects the bound: 0 for the minimum, 1 for the maximum. */
static int
rapl_sysctl_bound(SYSCTL_HANDLER_ARGS)
{
	struct rapl_softc *sc;
	uint32_t val;
	int error;

	sc = arg1;

	sx_xlock(&sc->sc_sx);
	val = arg2 == 0 ? sc->sc_min_allowed_mw : sc->sc_max_allowed_mw;
	error = sysctl_handle_32(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		goto out;
	if (arg2 == 0) {
		if (val == 0 || (sc->sc_max_allowed_mw != 0 &&
		    val > sc->sc_max_allowed_mw))
			error = EINVAL;
		else
			sc->sc_min_allowed_mw = val;
	} else {
		if (val != 0 && val < sc->sc_min_allowed_mw)
			error = EINVAL;
		else
			sc->sc_max_allowed_mw = val;
	}
out:
	sx_xunlock(&sc->sc_sx);
	return (error);
}

static int
rapl_sysctl_energy(SYSCTL_HANDLER_ARGS)
{
	struct rapl_softc *sc;
	uint64_t ticks;

	sc = arg1;
	mtx_lock_spin(&sc->sc_mtx);
	ticks = sc->sc_energy_ticks;
	mtx_unlock_spin(&sc->sc_mtx);
	ticks = rapl_ticks_to_uj(ticks, sc->sc_energy_shift);
	return (sysctl_handle_64(oidp, &ticks, 0, req));
}

static int
rapl_sysctl_power(SYSCTL_HANDLER_ARGS)
{
	struct rapl_softc *sc;
	uint32_t mw;

	sc = arg1;
	mtx_lock_spin(&sc->sc_mtx);
	mw = sc->sc_power_mw;
	mtx_unlock_spin(&sc->sc_mtx);
	return (sysctl_handle_32(oidp, &mw, 0, req));
}

/*
 * Once a second, fold the 32-bit energy counter into a 64-bit total and
 * work out the average power over the last interval.  The counter takes
 * minutes to wrap even at high power, so one sample per second never misses
 * a wrap.  Runs with sc_mtx held, from the clock interrupt of sc_cpu.
 */
static void
rapl_tick(void *arg)
{
	struct rapl_softc *sc;
	sbintime_t now;
	uint32_t delta, raw;

	sc = arg;
	mtx_assert(&sc->sc_mtx, MA_OWNED);
	if (sc->sc_detaching)
		return;

	if (PCPU_GET(cpuid) == sc->sc_cpu) {
		raw = (uint32_t)rdmsr(MSR_PKG_ENERGY_STATUS);
		now = sbinuptime();
		delta = raw - sc->sc_last_raw;
		sc->sc_energy_ticks += delta;
		sc->sc_power_mw = rapl_power_mw(
		    rapl_ticks_to_uj(delta, sc->sc_energy_shift),
		    (uint64_t)((now - sc->sc_last_sbt) / SBT_1US));
		sc->sc_last_raw = raw;
		sc->sc_last_sbt = now;
	}
	callout_reset_sbt_on(&sc->sc_callout, SBT_1S, 0, rapl_tick, sc,
	    sc->sc_cpu, C_DIRECT_EXEC);
}

static void
rapl_add_sysctls(struct rapl_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;

	/*
	 * A private context, so that detach can remove the nodes, and wait
	 * for running handlers, before the locks they use are destroyed.
	 */
	ctx = &sc->sc_sysctl_ctx;
	sysctl_ctx_init(ctx);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev));

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "package", CTLFLAG_RD,
	    &sc->sc_pkg, 0, "Package number");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cpu", CTLFLAG_RD,
	    &sc->sc_cpu, 0, "CPU used to access the package registers");
	SYSCTL_ADD_U32(ctx, children, OID_AUTO, "tdp_mw", CTLFLAG_RD,
	    &sc->sc_tdp_mw, 0, "Thermal design power (mW), 0 if not reported");
	SYSCTL_ADD_U32(ctx, children, OID_AUTO, "hw_min_mw", CTLFLAG_RD,
	    &sc->sc_hw_min_mw, 0,
	    "Minimum power reported by the hardware (mW), 0 if not reported");
	SYSCTL_ADD_U32(ctx, children, OID_AUTO, "hw_max_mw", CTLFLAG_RD,
	    &sc->sc_hw_max_mw, 0,
	    "Maximum power reported by the hardware (mW), 0 if not reported");
	SYSCTL_ADD_U64(ctx, children, OID_AUTO, "hw_max_window_us", CTLFLAG_RD,
	    &sc->sc_max_window_us, 0,
	    "Longest time window reported by the hardware (us), 0 if not "
	    "reported");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "locked", CTLFLAG_RD,
	    &sc->sc_locked, 0, "Firmware has locked the power limit register");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pl1_mw",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, RAPL_PL1, rapl_sysctl_limit, "IU",
	    "Sustained power limit (PL1, mW)");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pl2_mw",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, RAPL_PL2, rapl_sysctl_limit, "IU",
	    "Burst power limit (PL2, mW)");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pl1_enabled",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, RAPL_PL1, rapl_sysctl_enable, "I",
	    "Sustained power limit is enforced");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pl2_enabled",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, RAPL_PL2, rapl_sysctl_enable, "I",
	    "Burst power limit is enforced");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pl1_window_us",
	    CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, RAPL_PL1, rapl_sysctl_window, "QU",
	    "Time window of the sustained power limit (us)");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pl2_window_us",
	    CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, RAPL_PL2, rapl_sysctl_window, "QU",
	    "Time window of the burst power limit (us)");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "min_allowed_mw",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, 0, rapl_sysctl_bound, "IU",
	    "Lowest power limit this driver accepts (mW)");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "max_allowed_mw",
	    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_SECURE | CTLFLAG_MPSAFE,
	    sc, 1, rapl_sysctl_bound, "IU",
	    "Highest power limit this driver accepts (mW), 0 for no limit");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "energy_uj",
	    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, rapl_sysctl_energy, "QU",
	    "Package energy consumed since attach (uJ)");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "power_mw",
	    CTLTYPE_U32 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, rapl_sysctl_power, "IU",
	    "Average package power over the last second (mW)");
}

static int
rapl_attach(device_t dev)
{
	struct rapl_softc *sc;
	struct pcpu *pc;
	uint64_t energy, info, limit, units;
	bool wasbound;
	int error, oldcpu;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);
	sc->sc_cpu = pc->pc_cpuid;
	sc->sc_pkg = rapl_pkg_id(pc, rapl_pkg_shift());

	rapl_bind(sc, &oldcpu, &wasbound);
	error = rdmsr_safe(MSR_RAPL_POWER_UNIT, &units);
	if (error == 0)
		error = rdmsr_safe(MSR_PKG_POWER_LIMIT, &limit);
	if (error == 0)
		error = rdmsr_safe(MSR_PKG_ENERGY_STATUS, &energy);
	if (error == 0 && rdmsr_safe(MSR_PKG_POWER_INFO, &info) != 0)
		info = 0;
	rapl_unbind(oldcpu, wasbound);

	/*
	 * A hypervisor may return zero for MSRs it does not implement.  No
	 * real part has an energy unit of 1 J.
	 */
	if (error != 0 || units == 0 || units == ~0ULL ||
	    RAPL_UNIT_ENERGY(units) == 0) {
		if (bootverbose)
			device_printf(dev, "RAPL registers not available\n");
		return (ENXIO);
	}

	sc->sc_power_shift = RAPL_UNIT_POWER(units);
	sc->sc_energy_shift = RAPL_UNIT_ENERGY(units);
	sc->sc_time_shift = RAPL_UNIT_TIME(units);

	sc->sc_tdp_mw = (uint32_t)rapl_units_to_mw(RAPL_INFO_TDP(info),
	    sc->sc_power_shift);
	sc->sc_hw_min_mw = (uint32_t)rapl_units_to_mw(RAPL_INFO_MIN(info),
	    sc->sc_power_shift);
	sc->sc_hw_max_mw = (uint32_t)rapl_units_to_mw(RAPL_INFO_MAX(info),
	    sc->sc_power_shift);
	sc->sc_max_window_us = RAPL_INFO_WINDOW(info) != 0 ?
	    rapl_window_to_us(RAPL_INFO_WINDOW(info), sc->sc_time_shift) : 0;

	/*
	 * Seed the accepted range from the hardware where it reports one.
	 * Without a reported maximum, allow up to twice the TDP; without a
	 * TDP either, only the width of the register field limits it.
	 */
	sc->sc_min_allowed_mw = sc->sc_hw_min_mw != 0 ? sc->sc_hw_min_mw :
	    RAPL_DEFAULT_MIN_MW;
	sc->sc_max_allowed_mw = sc->sc_hw_max_mw != 0 ? sc->sc_hw_max_mw :
	    sc->sc_tdp_mw * 2;
	sc->sc_locked = (limit & RAPL_PL_LOCKED) != 0;

	sx_init(&sc->sc_sx, "intel_rapl limit");
	mtx_init(&sc->sc_mtx, "intel_rapl sample", NULL, MTX_SPIN);
	callout_init_mtx(&sc->sc_callout, &sc->sc_mtx, 0);
	sc->sc_last_raw = (uint32_t)energy;
	sc->sc_last_sbt = sbinuptime();

	rapl_add_sysctls(sc);

	mtx_lock_spin(&sc->sc_mtx);
	callout_reset_sbt_on(&sc->sc_callout, SBT_1S, 0, rapl_tick, sc,
	    sc->sc_cpu, C_DIRECT_EXEC);
	mtx_unlock_spin(&sc->sc_mtx);

	if (bootverbose || sc->sc_locked)
		device_printf(dev, "package %u, TDP %u mW%s\n", sc->sc_pkg,
		    sc->sc_tdp_mw, sc->sc_locked ?
		    ", power limits locked by firmware" : "");
	return (0);
}

static int
rapl_detach(device_t dev)
{
	struct rapl_softc *sc;

	sc = device_get_softc(dev);

	if (sysctl_ctx_free(&sc->sc_sysctl_ctx) != 0)
		return (EBUSY);

	mtx_lock_spin(&sc->sc_mtx);
	sc->sc_detaching = true;
	callout_stop(&sc->sc_callout);
	mtx_unlock_spin(&sc->sc_mtx);
	callout_drain(&sc->sc_callout);

	mtx_destroy(&sc->sc_mtx);
	sx_destroy(&sc->sc_sx);
	return (0);
}
