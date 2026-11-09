/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * Copyright (c) 2021, Intel Corporation.
 *
 * The table layout, the package-level interrupt handling and the enable and
 * disable sequences follow Intel's reference Hardware Feedback Interface
 * driver (github.com/intel/intel_hfi, by Aubrey Li and Ricardo Neri), which
 * Intel released under the GPL-2.0 OR BSD-3-Clause licenses and which is
 * used here under the BSD-3-Clause license.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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
 * Intel Hardware Feedback Interface (HFI) driver, hfi(4).
 *
 * The processor writes a table of per-core performance and energy-efficiency
 * capabilities (0..255, 0 meaning "do not schedule here") into memory that
 * software hands it, one table per physical package, and raises the local
 * APIC thermal interrupt on every CPU of the package when the table changes.
 * This driver owns those tables and publishes the capabilities as dynamic
 * scores through cpucap(9), where SCHED_MIC can read them.
 *
 * Table layout (Intel SDM vol. 3, "Hardware Feedback Interface"):
 *
 *	offset 0	uint64_t timestamp (TSC units) of the last update
 *	offset 8	header: one "changed" byte per capability and class,
 *			padded to 8 bytes
 *	offset 8+hdr	rows: one byte per capability and class, padded to
 *			8 bytes, indexed by CPUID.06H:EDX[31:16] as read ON
 *			THAT CPU
 *
 * With Thread Director disabled there is one class, so a row is the perf
 * byte, the efficiency byte and padding.  With it enabled the row holds
 * that pair once per class, classes contiguous (class k's perf byte at
 * k * ncaps), and the header likewise; on the parts so far (4 classes, 2
 * capabilities) both are still 8 bytes.
 *
 * Several logical CPUs share a row: SMT siblings and, on Alder Lake E-core
 * modules, all four cores of a module.  The row index therefore has to be
 * read on each CPU and may not be derived from the CPU number.
 *
 * Contexts
 *
 * hfi_intr() runs from the thermal interrupt vector with interrupts
 * disabled.  It only reads MSRs, copies the hardware table into a shadow
 * under a spin mutex, acknowledges the update and arms a callout.  The
 * callout enqueues hfi_update_task() on taskqueue_thread, which parses the
 * shadow and calls cpucap(9).  MSR programming happens on one "leader" CPU
 * per package via smp_rendezvous_cpus() from the attach, detach, suspend and
 * resume methods, all of which run in thread context.
 *
 * Thread Director (per-thread classes, IA32_HW_FEEDBACK_CONFIG bit 1) is
 * enabled only when the hw.hfi.itd tunable is set and the processor
 * reports it.  The per-CPU side (IA32_HW_FEEDBACK_THREAD_CONFIG,
 * IA32_HRESET_ENABLE) is programmed through itd_enable() in
 * sys/x86/x86/itd.c before the package bit is set, following the order
 * Intel's Linux patches use, and torn down on suspend.  The per-class
 * columns are published as class scores through cpucap(9); class 0 is the
 * plain score every classless consumer sees.
 *
 * Table lifetime
 *
 * Hardware tables are never freed.  Intel's reference driver notes that
 * some processors keep writing to, or later return to, the table address
 * they were first given even after HFI has been disabled or the pointer
 * reprogrammed, so handing those pages back to the allocator could corrupt
 * whatever gets them next.  The tables live in a file-scope cache keyed by
 * package id for the rest of the boot: a failed and retried attach reuses
 * the same pages for the same package, and detach is always refused, which
 * also keeps the module from being unloaded (kldunload fails with EBUSY
 * rather than detaching).  Shadows and mutexes
 * are ordinary softc resources and are freed normally.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/cpucap.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/intr_machdep.h>	/* For x86/apicvar.h */
#include <machine/specialreg.h>
#include <x86/apicvar.h>
#include <x86/itd.h>

/* CPUID.06H:EDX fields. */
#define	HFI_EDX_CAPS(edx)	((edx) & 0xff)
#define	HFI_CAP_PERF		0x01
#define	HFI_CAP_EE		0x02
#define	HFI_EDX_PAGES(edx)	((((edx) >> 8) & 0xf) + 1)
#define	HFI_EDX_INDEX(edx)	(((edx) >> 16) & 0xffff)

/* Largest table the 4-bit page count can describe. */
#define	HFI_MAX_PAGES		16

/* Hardware tables ever allocated; see "Table lifetime" above. */
struct hfi_hwtable {
	uint32_t	ht_id;		/* Package id. */
	size_t		ht_size;
	void		*ht_va;
	vm_paddr_t	ht_pa;
};
static struct hfi_hwtable hfi_hwtables[MAXCPU];	/* Packages <= CPUs. */
static int hfi_nhwtables;

/*
 * IA32_PACKAGE_THERM_STATUS write mask used to acknowledge an HFI update.
 *
 * Bits 1, 3, 5, 7, 9 and 11 are the thermal log bits and bit 26 is the HFI
 * update bit; all are RW0C (writing 1 leaves them alone, writing 0 clears
 * them) and every other bit is read-only.  Writing exactly this value
 * therefore clears only bit 26, without the read-modify-write race that
 * could clear a log bit which set between the read and the write.  Nothing
 * else in the tree writes this MSR: coretemp(4) uses the core-level
 * IA32_THERM_STATUS.
 */
#define	HFI_PKG_THERM_STATUS_KEEP_LOG	0x0aaaULL

/* How long to wait for the processor to acknowledge disabling HFI. */
#define	HFI_DISABLE_WAIT_US	2000

struct hfi_softc;

struct hfi_pkg {
	struct hfi_softc *hp_sc;
	uint32_t	hp_id;		/* Package id from CPUID. */
	int		hp_leader;	/* CPU that programs the MSRs. */
	void		*hp_table;	/* Hardware table, never freed. */
	vm_paddr_t	hp_table_pa;
	uint8_t		*hp_shadow;	/* Copy taken by the interrupt. */
	struct mtx	hp_lock;	/* Spin; shadow and timestamp. */
	uint64_t	hp_timestamp;	/* Timestamp of the shadow. */
	u_int		hp_pending;	/* One CPU handles the update. */
	u_int		hp_intrs;	/* Thermal interrupts seen. */
	u_int		hp_updates;	/* Shadow refreshes. */
	uint8_t		hp_perf_changed; /* Header change indications. */
	uint8_t		hp_ee_changed;
};

struct hfi_cpu {
	uint32_t	hc_edx;		/* Raw CPUID.06H:EDX on this CPU. */
	uint32_t	hc_pkgid;	/* Raw package id on this CPU. */
	int16_t		hc_index;	/* Row index, -1 if unusable. */
	int16_t		hc_pkg;		/* Index into sc_pkgs, -1 if none. */
	uint8_t		hc_perf[CPUCAP_MAX_CLASSES]; /* Last scores parsed, */
	uint8_t		hc_eff[CPUCAP_MAX_CLASSES];  /* per class. */
};

struct hfi_softc {
	device_t	sc_dev;
	size_t		sc_table_size;	/* Bytes per package table. */
	u_int		sc_pages;
	u_int		sc_ncaps;
	u_int		sc_hdr_size;
	u_int		sc_stride;
	u_int		sc_nrows;
	u_int		sc_hwclasses;	/* Classes in the table layout. */
	u_int		sc_nclasses;	/* Classes parsed and published. */
	struct hfi_pkg	*sc_pkgs;
	int		sc_npkgs;
	struct hfi_cpu	sc_cpu[MAXCPU];
	struct callout	sc_callout;
	struct task	sc_task;
	u_int		sc_delay_ms;
	u_int		sc_commits;
	bool		sc_enabled;	/* Tables programmed. */
	bool		sc_itd;		/* Thread Director enabled. */
	bool		sc_thermal;	/* lapic thermal handler installed. */
	bool		sc_registered;	/* cpucap provider registered. */
};

static MALLOC_DEFINE(M_HFI, "hfi", "Intel HFI tables");

/* hw.hfi.itd: enable Thread Director classes when the processor has them. */
static int hfi_itd_tunable = 0;
SYSCTL_NODE(_hw, OID_AUTO, hfi, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Intel Hardware Feedback Interface");
SYSCTL_INT(_hw_hfi, OID_AUTO, itd, CTLFLAG_RDTUN, &hfi_itd_tunable, 0,
    "Enable Intel Thread Director per-thread classes at attach (0 = off)");

/* For the cpucap provider callbacks, which take no argument. */
static struct hfi_softc *hfi_sc;

static int	hfi_cpucap_probe(void);
static int	hfi_cpucap_init(void);

static struct cpucap_provider hfi_provider = {
	.cp_name = "hfi",
	.cp_kind = CPUCAP_PROVIDER_DYNAMIC,
	.cp_priority = 10,
	.cp_probe = hfi_cpucap_probe,
	.cp_init = hfi_cpucap_init,
};

/*
 * CPUID helpers.  These run on the CPU being described.
 */

static bool
hfi_cpuid_supported(uint32_t *edxp)
{
	u_int regs[4];

	if (cpu_vendor_id != CPU_VENDOR_INTEL || cpu_high < 6)
		return (false);
	do_cpuid(6, regs);
	if (edxp != NULL)
		*edxp = regs[3];
	if ((regs[0] & CPUTPM1_HW_FEEDBACK) == 0)
		return (false);
	return ((HFI_EDX_CAPS(regs[3]) & (HFI_CAP_PERF | HFI_CAP_EE)) ==
	    (HFI_CAP_PERF | HFI_CAP_EE));
}

/*
 * Physical package id of the running CPU: its x2APIC id shifted by the
 * width of the highest topology level CPUID leaf 1FH (or 0BH) enumerates.
 * Falls back to one package when neither leaf is usable.
 */
static uint32_t
hfi_cpuid_package(void)
{
	u_int regs[4], leaf, level, shift;

	if (cpu_high >= 0x1f) {
		cpuid_count(0x1f, 0, regs);
		leaf = ((regs[2] >> 8) & 0xff) != 0 ? 0x1f : 0xb;
	} else
		leaf = 0xb;
	if (cpu_high < leaf)
		return (0);
	shift = 0;
	for (level = 0; level < 16; level++) {
		cpuid_count(leaf, level, regs);
		if (((regs[2] >> 8) & 0xff) == 0)
			break;
		shift = regs[0] & 0x1f;
	}
	/* EDX holds the x2APIC id for every sub-leaf, valid or not. */
	return (regs[3] >> shift);
}

/* smp_rendezvous action: record this CPU's row index and package. */
static void
hfi_cpu_probe(void *arg)
{
	struct hfi_softc *sc = arg;
	struct hfi_cpu *hc;
	uint32_t edx;
	u_int cpu;

	cpu = PCPU_GET(cpuid);
	if (cpu >= MAXCPU)
		return;
	hc = &sc->sc_cpu[cpu];
	hc->hc_pkg = -1;
	hc->hc_index = -1;
	hc->hc_pkgid = hfi_cpuid_package();
	if (!hfi_cpuid_supported(&edx)) {
		hc->hc_edx = edx;
		return;
	}
	hc->hc_edx = edx;
	if (HFI_EDX_INDEX(edx) < sc->sc_nrows)
		hc->hc_index = HFI_EDX_INDEX(edx);
}

/*
 * Package-level MSR programming.  Each runs on the package's leader CPU.
 */

static void
hfi_run_on_cpu(int cpu, void (*fn)(void *), void *arg)
{
	cpuset_t set;

	CPU_SETOF(cpu, &set);
	smp_rendezvous_cpus(set, NULL, fn, NULL, arg);
}

static void
hfi_pkg_enable(void *arg)
{
	struct hfi_pkg *pkg = arg;
	uint64_t msr;
	bool itd = pkg->hp_sc->sc_itd;

	/* Interrupt first, so the initial table write is not missed. */
	msr = rdmsr(MSR_IA32_PKG_THERM_INTERRUPT);
	wrmsr(MSR_IA32_PKG_THERM_INTERRUPT,
	    msr | IA32_PKG_THERM_INTERRUPT_HFI_ENABLE);

	wrmsr(MSR_IA32_HW_FEEDBACK_PTR,
	    (uint64_t)pkg->hp_table_pa | IA32_HW_FEEDBACK_PTR_ENABLE);

	msr = rdmsr(MSR_IA32_HW_FEEDBACK_CONFIG);
	msr &= ~IA32_HW_FEEDBACK_CONFIG_EN_THDIR;
	msr |= IA32_HW_FEEDBACK_CONFIG_EN_HFI;
	if (itd)
		msr |= IA32_HW_FEEDBACK_CONFIG_EN_THDIR;
	wrmsr(MSR_IA32_HW_FEEDBACK_CONFIG, msr);
}

static void
hfi_pkg_disable(void *arg)
{
	uint64_t msr;
	u_int i;

	msr = rdmsr(MSR_IA32_PKG_THERM_INTERRUPT);
	wrmsr(MSR_IA32_PKG_THERM_INTERRUPT,
	    msr & ~IA32_PKG_THERM_INTERRUPT_HFI_ENABLE);

	msr = rdmsr(MSR_IA32_HW_FEEDBACK_CONFIG);
	msr &= ~(IA32_HW_FEEDBACK_CONFIG_EN_HFI |
	    IA32_HW_FEEDBACK_CONFIG_EN_THDIR);
	wrmsr(MSR_IA32_HW_FEEDBACK_CONFIG, msr);

	/*
	 * The processor sets the HFI update bit once it has finished with
	 * the table.  Some parts never do; do not wait forever for them.
	 */
	for (i = 0; i < HFI_DISABLE_WAIT_US; i++) {
		if ((rdmsr(MSR_IA32_PKG_THERM_STATUS) &
		    IA32_PKG_THERM_STATUS_HFI_UPDATED) != 0)
			break;
		DELAY(1);
	}
	wrmsr(MSR_IA32_PKG_THERM_STATUS, HFI_PKG_THERM_STATUS_KEEP_LOG);
	wrmsr(MSR_IA32_HW_FEEDBACK_PTR, 0);
}

/*
 * Row geometry for the number of classes in the table layout.  The row
 * count can only grow when classes shrink, so indices already validated
 * against an ITD layout stay valid after a fall-back to the classless one.
 */
static void
hfi_geometry(struct hfi_softc *sc, u_int hwclasses, u_int nclasses)
{

	sc->sc_hwclasses = hwclasses;
	sc->sc_nclasses = nclasses;
	sc->sc_hdr_size = roundup2(sc->sc_ncaps * hwclasses, 8);
	sc->sc_stride = roundup2(sc->sc_ncaps * hwclasses, 8);
	sc->sc_nrows = (sc->sc_table_size - sizeof(uint64_t) -
	    sc->sc_hdr_size) / sc->sc_stride;
}

/*
 * Thread Director is enabled per logical CPU first, then per package; if
 * the per-CPU step fails the driver falls back to classless HFI and the
 * classless layout.  Also used on resume.
 */
static void
hfi_enable_all(struct hfi_softc *sc)
{
	int error, i;

	if (sc->sc_itd) {
		error = itd_enable(sc->sc_nclasses);
		if (error != 0) {
			device_printf(sc->sc_dev, "Thread Director per-CPU "
			    "enable failed: %d\n", error);
			sc->sc_itd = false;
			hfi_geometry(sc, 1, 1);
		}
	}
	for (i = 0; i < sc->sc_npkgs; i++)
		hfi_run_on_cpu(sc->sc_pkgs[i].hp_leader, hfi_pkg_enable,
		    &sc->sc_pkgs[i]);
	sc->sc_enabled = true;
}

static void
hfi_disable_all(struct hfi_softc *sc)
{
	int i;

	if (!sc->sc_enabled)
		return;
	for (i = 0; i < sc->sc_npkgs; i++)
		hfi_run_on_cpu(sc->sc_pkgs[i].hp_leader, hfi_pkg_disable,
		    NULL);
	itd_disable();
	sc->sc_enabled = false;
}

/*
 * Table handling.
 */

/*
 * Copy the hardware table into the shadow if its timestamp has moved.
 * Returns true if the shadow changed.  Safe in interrupt context.  The
 * copy is repeated if the processor wrote the table while it was in
 * progress, which can only happen in the polled (task) path: between an
 * interrupt and its acknowledgement the table is stable.
 */
static bool
hfi_pkg_snapshot(struct hfi_pkg *pkg)
{
	struct hfi_softc *sc = pkg->hp_sc;
	volatile uint64_t *hw_ts;
	uint64_t ts, ts2;
	bool changed;
	int tries;

	hw_ts = pkg->hp_table;
	changed = false;
	mtx_lock_spin(&pkg->hp_lock);
	ts = *hw_ts;
	if (ts != pkg->hp_timestamp) {
		for (tries = 0; tries < 3; tries++) {
			memcpy(pkg->hp_shadow, pkg->hp_table,
			    sc->sc_table_size);
			ts2 = *hw_ts;
			if (ts2 == ts)
				break;
			ts = ts2;
		}
		pkg->hp_timestamp = ts;
		pkg->hp_updates++;
		changed = true;
	}
	mtx_unlock_spin(&pkg->hp_lock);
	return (changed);
}

/*
 * Parse the shadow of one package into the per-CPU, per-class scores.
 * Every row access is bounds-checked against the table size, although the
 * index was already checked against sc_nrows when it was read, and the
 * class loop is bounded by both the softc count and the array size.
 */
static void
hfi_pkg_parse(struct hfi_softc *sc, int pkgidx)
{
	struct hfi_pkg *pkg = &sc->sc_pkgs[pkgidx];
	struct hfi_cpu *hc;
	const uint8_t *hdr, *row;
	size_t off;
	u_int cls, cpu;

	mtx_lock_spin(&pkg->hp_lock);
	/* An all-zero shadow would read as "avoid every CPU"; wait. */
	if (pkg->hp_updates == 0) {
		mtx_unlock_spin(&pkg->hp_lock);
		return;
	}
	hdr = pkg->hp_shadow + sizeof(uint64_t);
	pkg->hp_perf_changed = hdr[0];
	pkg->hp_ee_changed = hdr[1];
	for (cpu = 0; cpu <= mp_maxid && cpu < MAXCPU; cpu++) {
		hc = &sc->sc_cpu[cpu];
		if (hc->hc_pkg != pkgidx || hc->hc_index < 0)
			continue;
		off = sizeof(uint64_t) + sc->sc_hdr_size +
		    (size_t)hc->hc_index * sc->sc_stride;
		if (off + sc->sc_stride > sc->sc_table_size)
			continue;
		row = pkg->hp_shadow + off;
		for (cls = 0; cls < sc->sc_nclasses &&
		    cls < CPUCAP_MAX_CLASSES; cls++) {
			/* Class k's pair starts at k * ncaps within the row. */
			if ((size_t)cls * sc->sc_ncaps + 1 >= sc->sc_stride)
				break;
			hc->hc_perf[cls] = row[cls * sc->sc_ncaps];
			hc->hc_eff[cls] = row[cls * sc->sc_ncaps + 1];
		}
	}
	mtx_unlock_spin(&pkg->hp_lock);
}

/* Publish the parsed scores.  The caller ensures this provider is active. */
static void
hfi_publish(struct hfi_softc *sc)
{
	struct hfi_cpu *hc;
	u_int cls, cpu;

	cpucap_set_nclasses(sc->sc_nclasses);
	for (cpu = 0; cpu <= mp_maxid && cpu < MAXCPU; cpu++) {
		hc = &sc->sc_cpu[cpu];
		if (hc->hc_index < 0)
			continue;
		for (cls = 0; cls < sc->sc_nclasses &&
		    cls < CPUCAP_MAX_CLASSES; cls++)
			cpucap_set_class_scores(cpu, cls, hc->hc_perf[cls],
			    hc->hc_eff[cls]);
	}
}

/*
 * Taskqueue task: refresh any shadow the interrupt did not (the first
 * table write at boot, a lost interrupt, resume), parse and publish.
 */
static void
hfi_update_task(void *arg, int pending __unused)
{
	struct hfi_softc *sc = arg;
	int i;

	for (i = 0; i < sc->sc_npkgs; i++) {
		(void)hfi_pkg_snapshot(&sc->sc_pkgs[i]);
		hfi_pkg_parse(sc, i);
	}
	if (hfi_provider.cp_state != CPUCAP_PS_ACTIVE)
		return;
	hfi_publish(sc);
	cpucap_commit();
	sc->sc_commits++;
}

/* Callout: batches interrupts, then hands over to thread context. */
static void
hfi_callout(void *arg)
{
	struct hfi_softc *sc = arg;

	taskqueue_enqueue(taskqueue_thread, &sc->sc_task);
}

static void
hfi_schedule(struct hfi_softc *sc)
{
	int ticks;

	if (callout_pending(&sc->sc_callout))
		return;
	ticks = MAX(1, (int)(sc->sc_delay_ms * hz / 1000));
	callout_reset(&sc->sc_callout, ticks, hfi_callout, sc);
}

/*
 * Thermal interrupt.  Runs with interrupts disabled on every CPU of the
 * package; one of them takes the update, the rest return.  This is also
 * reached for thermal events other than HFI, in which case bit 26 is
 * clear and nothing is acknowledged.
 */
static void
hfi_intr(int cpu, void *arg)
{
	struct hfi_softc *sc = arg;
	struct hfi_pkg *pkg;
	uint64_t status;
	bool changed;

	if (cpu < 0 || cpu >= MAXCPU || sc->sc_cpu[cpu].hc_pkg < 0)
		return;
	pkg = &sc->sc_pkgs[sc->sc_cpu[cpu].hc_pkg];
	atomic_add_int(&pkg->hp_intrs, 1);

	status = rdmsr(MSR_IA32_PKG_THERM_STATUS);
	if ((status & IA32_PKG_THERM_STATUS_HFI_UPDATED) == 0)
		return;
	if (atomic_cmpset_acq_int(&pkg->hp_pending, 0, 1) == 0)
		return;

	changed = hfi_pkg_snapshot(pkg);
	/*
	 * Tell the processor the table has been read; it may write it
	 * again from here on.  Clears only bit 26, see the mask.
	 */
	wrmsr(MSR_IA32_PKG_THERM_STATUS, HFI_PKG_THERM_STATUS_KEEP_LOG);
	atomic_store_rel_int(&pkg->hp_pending, 0);

	if (changed)
		hfi_schedule(sc);
}

/*
 * cpucap(9) provider.
 */

static int
hfi_cpucap_probe(void)
{

	return (hfi_sc != NULL && hfi_sc->sc_enabled ? 0 : ENXIO);
}

/* Runs with the cpucap lock held: no sleeping, just republish the cache. */
static int
hfi_cpucap_init(void)
{

	if (hfi_sc == NULL)
		return (ENXIO);
	hfi_publish(hfi_sc);
	return (0);
}

/*
 * Resources.
 */

static void
hfi_free_pkgs(struct hfi_softc *sc)
{
	struct hfi_pkg *pkg;
	int i;

	if (sc->sc_pkgs == NULL)
		return;
	for (i = 0; i < sc->sc_npkgs; i++) {
		pkg = &sc->sc_pkgs[i];
		/* hp_table stays in hfi_hwtables; see "Table lifetime". */
		if (pkg->hp_shadow != NULL)
			free(pkg->hp_shadow, M_HFI);
		if (mtx_initialized(&pkg->hp_lock))
			mtx_destroy(&pkg->hp_lock);
	}
	free(sc->sc_pkgs, M_HFI);
	sc->sc_pkgs = NULL;
	sc->sc_npkgs = 0;
}

/*
 * Return the hardware table for a package, allocating it on first use.
 * Tables are page-aligned, zeroed and kept for the rest of the boot.
 */
static struct hfi_hwtable *
hfi_hwtable_get(uint32_t id, size_t size)
{
	struct hfi_hwtable *ht;
	int i;

	for (i = 0; i < hfi_nhwtables; i++) {
		ht = &hfi_hwtables[i];
		if (ht->ht_id == id && ht->ht_size == size)
			return (ht);
	}
	if (hfi_nhwtables >= nitems(hfi_hwtables))
		return (NULL);
	ht = &hfi_hwtables[hfi_nhwtables];
	ht->ht_va = contigmalloc(size, M_HFI, M_WAITOK | M_ZERO, 0,
	    ~(vm_paddr_t)0, PAGE_SIZE, 0);
	if (ht->ht_va == NULL)
		return (NULL);
	ht->ht_id = id;
	ht->ht_size = size;
	ht->ht_pa = vtophys(ht->ht_va);
	hfi_nhwtables++;
	return (ht);
}

/*
 * Group the CPUs that reported a usable row by package id, find or
 * allocate one hardware table and allocate one shadow per package, and
 * pick the lowest-numbered CPU of each as its leader.
 */
static int
hfi_alloc_pkgs(struct hfi_softc *sc)
{
	struct hfi_hwtable *ht;
	struct hfi_pkg *pkg;
	struct hfi_cpu *hc;
	u_int cpu;
	int i;

	/* One slot per CPU is an upper bound on the number of packages. */
	sc->sc_pkgs = malloc(sizeof(*sc->sc_pkgs) * (mp_maxid + 1), M_HFI,
	    M_WAITOK | M_ZERO);
	sc->sc_npkgs = 0;
	for (cpu = 0; cpu <= mp_maxid && cpu < MAXCPU; cpu++) {
		hc = &sc->sc_cpu[cpu];
		if (CPU_ABSENT(cpu) || hc->hc_index < 0)
			continue;
		for (i = 0; i < sc->sc_npkgs; i++)
			if (sc->sc_pkgs[i].hp_id == hc->hc_pkgid)
				break;
		if (i == sc->sc_npkgs) {
			pkg = &sc->sc_pkgs[sc->sc_npkgs++];
			pkg->hp_sc = sc;
			pkg->hp_id = hc->hc_pkgid;
			pkg->hp_leader = cpu;
			mtx_init(&pkg->hp_lock, "hfi table", NULL, MTX_SPIN);
			ht = hfi_hwtable_get(hc->hc_pkgid, sc->sc_table_size);
			if (ht == NULL)
				return (ENOMEM);
			pkg->hp_table = ht->ht_va;
			pkg->hp_table_pa = ht->ht_pa;
			pkg->hp_shadow = malloc(sc->sc_table_size, M_HFI,
			    M_WAITOK | M_ZERO);
		}
		hc->hc_pkg = i;
	}
	return (sc->sc_npkgs == 0 ? ENXIO : 0);
}

/*
 * sysctl.
 */

static int
hfi_sysctl_summary(SYSCTL_HANDLER_ARGS)
{
	struct hfi_softc *sc = arg1;
	struct hfi_cpu *hc;
	struct sbuf sb;
	u_int cls, cpu;
	int error;
	bool first;

	sbuf_new_for_sysctl(&sb, NULL, 128, req);
	first = true;
	for (cpu = 0; cpu <= mp_maxid && cpu < MAXCPU; cpu++) {
		hc = &sc->sc_cpu[cpu];
		if (hc->hc_pkg < 0)
			continue;
		sbuf_printf(&sb, "%scpu%u:%d:", first ? "" : " ", cpu,
		    hc->hc_index);
		for (cls = 0; cls < sc->sc_nclasses &&
		    cls < CPUCAP_MAX_CLASSES; cls++)
			sbuf_printf(&sb, "%s%u/%u", cls == 0 ? "" : ",",
			    hc->hc_perf[cls], hc->hc_eff[cls]);
		first = false;
	}
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

static void
hfi_sysctl_init(struct hfi_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *tree, *list;
	struct sysctl_oid *node, *sub;
	struct hfi_pkg *pkg;
	struct hfi_cpu *hc;
	char name[16];
	u_int cpu;
	int i;

	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev));

	SYSCTL_ADD_BOOL(ctx, tree, OID_AUTO, "enabled", CTLFLAG_RD,
	    &sc->sc_enabled, 0, "HFI tables programmed");
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "table_pages", CTLFLAG_RD,
	    &sc->sc_pages, 0, "Table size in 4 KB pages");
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "capabilities", CTLFLAG_RD,
	    &sc->sc_ncaps, 0, "Capabilities (columns) per row");
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "rows", CTLFLAG_RD,
	    &sc->sc_nrows, 0, "Rows the table can hold");
	SYSCTL_ADD_BOOL(ctx, tree, OID_AUTO, "itd", CTLFLAG_RD,
	    &sc->sc_itd, 0, "Thread Director enabled");
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "classes", CTLFLAG_RD,
	    &sc->sc_nclasses, 0, "Thread classes published (1 = class 0 only)");
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "packages", CTLFLAG_RD,
	    &sc->sc_npkgs, 0, "Packages with a table");
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "commits", CTLFLAG_RD,
	    &sc->sc_commits, 0, "Score batches committed to cpucap");
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "delay_ms", CTLFLAG_RWTUN,
	    &sc->sc_delay_ms, 0, "Delay between an update and publishing it");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "summary",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    hfi_sysctl_summary, "A",
	    "cpuN:row:perf/eff[,perf/eff per class] for every CPU");

	node = SYSCTL_ADD_NODE(ctx, tree, OID_AUTO, "pkg",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Per-package state");
	list = SYSCTL_CHILDREN(node);
	for (i = 0; i < sc->sc_npkgs; i++) {
		pkg = &sc->sc_pkgs[i];
		snprintf(name, sizeof(name), "%d", i);
		sub = SYSCTL_ADD_NODE(ctx, list, OID_AUTO, name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Package");
		SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "id",
		    CTLFLAG_RD, &pkg->hp_id, 0, "CPUID package id");
		SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "leader",
		    CTLFLAG_RD, &pkg->hp_leader, 0, "CPU that programs MSRs");
		SYSCTL_ADD_U64(ctx, SYSCTL_CHILDREN(sub), OID_AUTO,
		    "timestamp", CTLFLAG_RD, &pkg->hp_timestamp, 0,
		    "Timestamp of the last table update");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(sub), OID_AUTO,
		    "interrupts", CTLFLAG_RD, &pkg->hp_intrs, 0,
		    "Thermal interrupts seen");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(sub), OID_AUTO,
		    "updates", CTLFLAG_RD, &pkg->hp_updates, 0,
		    "Table updates copied");
		SYSCTL_ADD_U8(ctx, SYSCTL_CHILDREN(sub), OID_AUTO,
		    "perf_changed", CTLFLAG_RD, &pkg->hp_perf_changed, 0,
		    "Header: performance column changed");
		SYSCTL_ADD_U8(ctx, SYSCTL_CHILDREN(sub), OID_AUTO,
		    "ee_changed", CTLFLAG_RD, &pkg->hp_ee_changed, 0,
		    "Header: efficiency column changed");
	}

	node = SYSCTL_ADD_NODE(ctx, tree, OID_AUTO, "cpu",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Per-CPU state");
	list = SYSCTL_CHILDREN(node);
	for (cpu = 0; cpu <= mp_maxid && cpu < MAXCPU; cpu++) {
		hc = &sc->sc_cpu[cpu];
		if (CPU_ABSENT(cpu))
			continue;
		snprintf(name, sizeof(name), "%u", cpu);
		sub = SYSCTL_ADD_NODE(ctx, list, OID_AUTO, name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "CPU");
		SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "edx",
		    CTLFLAG_RD, &hc->hc_edx, 0, "Raw CPUID.06H:EDX");
		SYSCTL_ADD_S16(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "index",
		    CTLFLAG_RD, &hc->hc_index, 0, "Row index, -1 if none");
		SYSCTL_ADD_S16(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "pkg",
		    CTLFLAG_RD, &hc->hc_pkg, 0, "Package node, -1 if none");
		SYSCTL_ADD_U8(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "perf",
		    CTLFLAG_RD, &hc->hc_perf[0], 0, "Performance capability");
		SYSCTL_ADD_U8(ctx, SYSCTL_CHILDREN(sub), OID_AUTO, "eff",
		    CTLFLAG_RD, &hc->hc_eff[0], 0,
		    "Energy efficiency capability");
	}
}

/*
 * Newbus glue.  One device for the whole machine, attached below cpu0.
 */

static void
hfi_identify(driver_t *driver, device_t parent)
{

	if (device_get_unit(parent) != 0)
		return;
	if (device_find_child(parent, "hfi", -1) != NULL)
		return;
	if (!hfi_cpuid_supported(NULL))
		return;
	if (device_add_child(parent, "hfi", 0) == NULL)
		device_printf(parent, "add hfi child failed\n");
}

static int
hfi_probe(device_t dev)
{

	if (resource_disabled("hfi", 0))
		return (ENXIO);
	if (!hfi_cpuid_supported(NULL))
		return (ENXIO);
	device_set_desc(dev, "Intel Hardware Feedback Interface");
	return (BUS_PROBE_DEFAULT);
}

static int
hfi_attach(device_t dev)
{
	struct hfi_softc *sc;
	struct hfi_cpu *hc;
	uint32_t edx;
	u_int cls, cpu, nclasses, nvalid;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_delay_ms = 100;
	callout_init(&sc->sc_callout, 1);
	TASK_INIT(&sc->sc_task, 0, hfi_update_task, sc);

	if (mp_ncpus > 1 && !smp_started) {
		device_printf(dev, "APs not started yet\n");
		return (ENXIO);
	}
	if (hfi_sc != NULL) {
		device_printf(dev, "already attached\n");
		return (EEXIST);
	}

	/*
	 * Geometry from the boot CPU.  The table size and capability set
	 * are package-wide; hybrid parts report the same values on P and E
	 * cores, only the row index differs.
	 */
	if (!hfi_cpuid_supported(&edx))
		return (ENXIO);
	sc->sc_ncaps = bitcount32(HFI_EDX_CAPS(edx));
	sc->sc_pages = HFI_EDX_PAGES(edx);
	if (sc->sc_pages > HFI_MAX_PAGES)
		return (ENXIO);
	sc->sc_table_size = (size_t)sc->sc_pages * PAGE_SIZE;
	/*
	 * Thread Director: the layout gets one capability set per class
	 * that the processor reports, of which at most CPUCAP_MAX_CLASSES
	 * are parsed and published.  Off unless hw.hfi.itd is set.
	 */
	sc->sc_itd = false;
	if (hfi_itd_tunable != 0 && itd_supported(&nclasses) &&
	    nclasses >= 1) {
		hfi_geometry(sc, nclasses, MIN(nclasses, CPUCAP_MAX_CLASSES));
		sc->sc_itd = true;
	} else
		hfi_geometry(sc, 1, 1);
	if (sc->sc_nrows == 0)
		return (ENXIO);

	/* Every CPU reads its own row index. */
	for (cpu = 0; cpu < MAXCPU; cpu++) {
		sc->sc_cpu[cpu].hc_index = -1;
		sc->sc_cpu[cpu].hc_pkg = -1;
		for (cls = 0; cls < CPUCAP_MAX_CLASSES; cls++) {
			sc->sc_cpu[cpu].hc_perf[cls] = CPUCAP_SCORE_DEFAULT;
			sc->sc_cpu[cpu].hc_eff[cls] = CPUCAP_SCORE_DEFAULT;
		}
	}
	smp_rendezvous(NULL, hfi_cpu_probe, NULL, sc);
	nvalid = 0;
	for (cpu = 0; cpu <= mp_maxid && cpu < MAXCPU; cpu++) {
		hc = &sc->sc_cpu[cpu];
		if (CPU_ABSENT(cpu))
			continue;
		if (hc->hc_index >= 0) {
			nvalid++;
			continue;
		}
		device_printf(dev, "cpu%u: no usable row (edx 0x%08x, "
		    "%u rows)\n", cpu, hc->hc_edx, sc->sc_nrows);
	}
	if (nvalid == 0)
		return (ENXIO);

	error = hfi_alloc_pkgs(sc);
	if (error != 0) {
		hfi_free_pkgs(sc);
		return (error);
	}

	if (!lapic_enable_thermal(hfi_intr, sc)) {
		device_printf(dev, "no local APIC thermal LVT\n");
		hfi_free_pkgs(sc);
		return (ENXIO);
	}
	sc->sc_thermal = true;

	hfi_sc = sc;
	hfi_enable_all(sc);
	hfi_sysctl_init(sc);

	/* Initial read; some parts write the table only once, at boot. */
	hfi_schedule(sc);

	error = cpucap_provider_register(&hfi_provider);
	if (error != 0)
		device_printf(dev, "cpucap registration failed: %d\n", error);
	else
		sc->sc_registered = true;

	if (bootverbose)
		device_printf(dev, "%d package(s), %u-page table, %u rows, "
		    "%u CPUs with a row\n", sc->sc_npkgs, sc->sc_pages,
		    sc->sc_nrows, nvalid);
	if (sc->sc_itd)
		device_printf(dev, "Thread Director enabled, %u classes%s\n",
		    sc->sc_nclasses, itd_hreset_mask != 0 ? ", HRESET" : "");
	return (0);
}

static int
hfi_detach(device_t dev)
{

	/*
	 * Once the processor has been given a table it may keep using the
	 * pages, so the driver stays attached for the rest of the boot; this
	 * also makes kldunload fail with EBUSY.  See "Table lifetime".
	 */
	device_printf(dev, "cannot detach: HFI table is in use by the "
	    "processor\n");
	return (EBUSY);
}

static int
hfi_suspend(device_t dev)
{

	hfi_disable_all(device_get_softc(dev));
	return (0);
}

static int
hfi_resume(device_t dev)
{
	struct hfi_softc *sc;
	int i;

	sc = device_get_softc(dev);
	if (sc->sc_pkgs == NULL || !sc->sc_thermal)
		return (0);
	/* Force the next snapshot to re-read every table. */
	for (i = 0; i < sc->sc_npkgs; i++) {
		mtx_lock_spin(&sc->sc_pkgs[i].hp_lock);
		sc->sc_pkgs[i].hp_timestamp = 0;
		mtx_unlock_spin(&sc->sc_pkgs[i].hp_lock);
	}
	hfi_enable_all(sc);
	hfi_schedule(sc);
	return (0);
}

static device_method_t hfi_methods[] = {
	DEVMETHOD(device_identify,	hfi_identify),
	DEVMETHOD(device_probe,		hfi_probe),
	DEVMETHOD(device_attach,	hfi_attach),
	DEVMETHOD(device_detach,	hfi_detach),
	DEVMETHOD(device_suspend,	hfi_suspend),
	DEVMETHOD(device_resume,	hfi_resume),
	DEVMETHOD_END
};

static driver_t hfi_driver = {
	"hfi",
	hfi_methods,
	sizeof(struct hfi_softc),
};

static devclass_t hfi_devclass;
DRIVER_MODULE(hfi, cpu, hfi_driver, hfi_devclass, NULL, NULL);
MODULE_DEPEND(hfi, cpucap, 1, 1, 1);
MODULE_VERSION(hfi, 1);
