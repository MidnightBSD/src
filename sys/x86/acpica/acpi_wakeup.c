/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001-2012 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 2008-2012 Jung-uk Kim <jkim@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/x86/acpica/acpi_wakeup.c 255726 2013-09-20 05:06:03Z gibbs $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/clock.h>
#include <machine/intr_machdep.h>
#include <x86/mca.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>
#include <machine/md_var.h>

#ifdef SMP
#include <x86/apicreg.h>
#include <machine/smp.h>
#include <machine/vmparam.h>
#endif

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include "acpi_wakecode.h"
#include "acpi_wakedata.h"

/* Make sure the code is less than a page and leave room for the stack. */
CTASSERT(sizeof(wakecode) < PAGE_SIZE - 1024);

extern int		acpi_resume_beep;
extern int		acpi_reset_video;

#ifdef SMP
extern struct pcb	**susppcbs;
static cpuset_t		suspcpus;
#else
static struct pcb	**susppcbs;
#endif

static void		*acpi_alloc_wakeup_handler(void);
static void		acpi_stop_beep(void *);

#ifdef SMP
static int		acpi_wakeup_ap(struct acpi_softc *, int);
static void		acpi_wakeup_cpus(struct acpi_softc *);
#endif

#ifdef __amd64__
#define ACPI_PAGETABLES	3
#else
#define ACPI_PAGETABLES	0
#endif

#define	WAKECODE_VADDR(sc)				\
    ((sc)->acpi_wakeaddr + (ACPI_PAGETABLES * PAGE_SIZE))
#define	WAKECODE_PADDR(sc)				\
    ((sc)->acpi_wakephys + (ACPI_PAGETABLES * PAGE_SIZE))
#define	WAKECODE_FIXUP(offset, type, val)	do {	\
	type	*addr;					\
	addr = (type *)(WAKECODE_VADDR(sc) + offset);	\
	*addr = val;					\
} while (0)

static void
acpi_stop_beep(void *arg)
{

	if (acpi_resume_beep != 0)
		timer_spkr_release();
}

#ifdef SMP
static int
acpi_wakeup_ap(struct acpi_softc *sc, int cpu)
{
	int		vector = (WAKECODE_PADDR(sc) >> 12) & 0xff;
	int		apic_id = cpu_apic_ids[cpu];
	int		ms;

	WAKECODE_FIXUP(wakeup_pcb, struct pcb *, susppcbs[cpu]);
	WAKECODE_FIXUP(wakeup_gdt, uint16_t, susppcbs[cpu]->pcb_gdt.rd_limit);
	WAKECODE_FIXUP(wakeup_gdt + 2, uint64_t,
	    susppcbs[cpu]->pcb_gdt.rd_base);

	ipi_startup(apic_id, vector);

	/* Wait up to 5 seconds for it to resume. */
	for (ms = 0; ms < 5000; ms++) {
		if (!CPU_ISSET(cpu, &suspended_cpus))
			return (1);	/* return SUCCESS */
		DELAY(1000);
	}
	return (0);		/* return FAILURE */
}

#define	WARMBOOT_TARGET		0
#define	WARMBOOT_OFF		(KERNBASE + 0x0467)
#define	WARMBOOT_SEG		(KERNBASE + 0x0469)

#define	CMOS_REG		(0x70)
#define	CMOS_DATA		(0x71)
#define	BIOS_RESET		(0x0f)
#define	BIOS_WARM		(0x0a)

static void
acpi_wakeup_cpus(struct acpi_softc *sc)
{
	uint32_t	mpbioswarmvec;
	int		cpu;
	u_char		mpbiosreason;

	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((uint32_t *)WARMBOOT_OFF);
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);

	/* setup a vector to our boot code */
	*((volatile u_short *)WARMBOOT_OFF) = WARMBOOT_TARGET;
	*((volatile u_short *)WARMBOOT_SEG) = WAKECODE_PADDR(sc) >> 4;
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

	/* Wake up each AP. */
	for (cpu = 1; cpu < mp_ncpus; cpu++) {
		if (!CPU_ISSET(cpu, &suspcpus))
			continue;
		if (acpi_wakeup_ap(sc, cpu) == 0) {
			/* restore the warmstart vector */
			*(uint32_t *)WARMBOOT_OFF = mpbioswarmvec;
			panic("acpi_wakeup: failed to resume AP #%d (PHY #%d)",
			    cpu, cpu_apic_ids[cpu]);
		}
	}

	/* restore the warmstart vector */
	*(uint32_t *)WARMBOOT_OFF = mpbioswarmvec;

	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);
}
#endif

int
acpi_sleep_machdep(struct acpi_softc *sc, int state)
{
	ACPI_STATUS	status;

	if (sc->acpi_wakeaddr == 0ul)
		return (-1);	/* couldn't alloc wake memory */

#ifdef SMP
	suspcpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &suspcpus);
#endif

	if (acpi_resume_beep != 0)
		timer_spkr_acquire();

	AcpiSetFirmwareWakingVector(WAKECODE_PADDR(sc));

	intr_suspend();

	if (savectx(susppcbs[0])) {
#ifdef __amd64__
		ctx_fpusave(susppcbs[0]->pcb_fpususpend);
#endif
#ifdef SMP
		if (!CPU_EMPTY(&suspcpus) && suspend_cpus(suspcpus) == 0) {
			device_printf(sc->acpi_dev, "Failed to suspend APs\n");
			return (0);	/* couldn't sleep */
		}
#endif

		WAKECODE_FIXUP(resume_beep, uint8_t, (acpi_resume_beep != 0));
		WAKECODE_FIXUP(reset_video, uint8_t, (acpi_reset_video != 0));

#ifndef __amd64__
		WAKECODE_FIXUP(wakeup_cr4, register_t, susppcbs[0]->pcb_cr4);
#endif
		WAKECODE_FIXUP(wakeup_pcb, struct pcb *, susppcbs[0]);
		WAKECODE_FIXUP(wakeup_gdt, uint16_t,
		    susppcbs[0]->pcb_gdt.rd_limit);
		WAKECODE_FIXUP(wakeup_gdt + 2, uint64_t,
		    susppcbs[0]->pcb_gdt.rd_base);

		/* Call ACPICA to enter the desired sleep state */
		if (state == ACPI_STATE_S4 && sc->acpi_s4bios)
			status = AcpiEnterSleepStateS4bios();
		else
			status = AcpiEnterSleepState(state);
		if (ACPI_FAILURE(status)) {
			device_printf(sc->acpi_dev,
			    "AcpiEnterSleepState failed - %s\n",
			    AcpiFormatException(status));
			return (0);	/* couldn't sleep */
		}

		for (;;)
			ia32_pause();
	}

	return (1);	/* wakeup successfully */
}

int
acpi_wakeup_machdep(struct acpi_softc *sc, int state, int sleep_result,
    int intr_enabled)
{

	if (sleep_result == -1)
		return (sleep_result);

	if (!intr_enabled) {
		/* Wakeup MD procedures in interrupt disabled context */
		if (sleep_result == 1) {
			pmap_init_pat();
			initializecpu();
			PCPU_SET(switchtime, 0);
			PCPU_SET(switchticks, ticks);
#ifdef SMP
			if (!CPU_EMPTY(&suspcpus))
				acpi_wakeup_cpus(sc);
#endif
		}

#ifdef SMP
		if (!CPU_EMPTY(&suspcpus))
			restart_cpus(suspcpus);
#endif
		mca_resume();
		intr_resume(/*suspend_cancelled*/false);

		AcpiSetFirmwareWakingVector(0);
	} else {
		/* Wakeup MD procedures in interrupt enabled context */
		if (sleep_result == 1 && mem_range_softc.mr_op != NULL &&
		    mem_range_softc.mr_op->reinit != NULL)
			mem_range_softc.mr_op->reinit(&mem_range_softc);
	}

	return (sleep_result);
}

static void *
acpi_alloc_wakeup_handler(void)
{
	void		*wakeaddr;
	int		i;

	/*
	 * Specify the region for our wakeup code.  We want it in the low 1 MB
	 * region, excluding real mode IVT (0-0x3ff), BDA (0x400-0x4ff), EBDA
	 * (less than 128KB, below 0xa0000, must be excluded by SMAP and DSDT),
	 * and ROM area (0xa0000 and above).  The temporary page tables must be
	 * page-aligned.
	 */
	wakeaddr = contigmalloc((ACPI_PAGETABLES + 1) * PAGE_SIZE, M_DEVBUF,
	    M_WAITOK, 0x500, 0xa0000, PAGE_SIZE, 0ul);
	if (wakeaddr == NULL) {
		printf("%s: can't alloc wake memory\n", __func__);
		return (NULL);
	}
	if (EVENTHANDLER_REGISTER(power_resume, acpi_stop_beep, NULL,
	    EVENTHANDLER_PRI_LAST) == NULL) {
		printf("%s: can't register event handler\n", __func__);
		contigfree(wakeaddr, (ACPI_PAGETABLES + 1) * PAGE_SIZE,
		    M_DEVBUF);
		return (NULL);
	}
	susppcbs = malloc(mp_ncpus * sizeof(*susppcbs), M_DEVBUF, M_WAITOK);
	for (i = 0; i < mp_ncpus; i++) {
		susppcbs[i] = malloc(sizeof(**susppcbs), M_DEVBUF, M_WAITOK);
#ifdef __amd64__
		susppcbs[i]->pcb_fpususpend = alloc_fpusave(M_WAITOK);
#endif
	}

	return (wakeaddr);
}

void
acpi_install_wakeup_handler(struct acpi_softc *sc)
{
	static void	*wakeaddr = NULL;
#ifdef __amd64__
	uint64_t	*pt4, *pt3, *pt2;
	int		i;
#endif

	if (wakeaddr != NULL)
		return;

	wakeaddr = acpi_alloc_wakeup_handler();
	if (wakeaddr == NULL)
		return;

	sc->acpi_wakeaddr = (vm_offset_t)wakeaddr;
	sc->acpi_wakephys = vtophys(wakeaddr);

	bcopy(wakecode, (void *)WAKECODE_VADDR(sc), sizeof(wakecode));

	/* Patch GDT base address, ljmp targets. */
	WAKECODE_FIXUP((bootgdtdesc + 2), uint32_t,
	    WAKECODE_PADDR(sc) + bootgdt);
	WAKECODE_FIXUP((wakeup_sw32 + 2), uint32_t,
	    WAKECODE_PADDR(sc) + wakeup_32);
#ifdef __amd64__
	WAKECODE_FIXUP((wakeup_sw64 + 1), uint32_t,
	    WAKECODE_PADDR(sc) + wakeup_64);
	WAKECODE_FIXUP(wakeup_pagetables, uint32_t, sc->acpi_wakephys);
#endif

	/* Save pointers to some global data. */
	WAKECODE_FIXUP(wakeup_ret, void *, resumectx);
#ifndef __amd64__
#ifdef PAE
	WAKECODE_FIXUP(wakeup_cr3, register_t, vtophys(kernel_pmap->pm_pdpt));
#else
	WAKECODE_FIXUP(wakeup_cr3, register_t, vtophys(kernel_pmap->pm_pdir));
#endif

#else
	/* Build temporary page tables below realmode code. */
	pt4 = wakeaddr;
	pt3 = pt4 + (PAGE_SIZE) / sizeof(uint64_t);
	pt2 = pt3 + (PAGE_SIZE) / sizeof(uint64_t);

	/* Create the initial 1GB replicated page tables */
	for (i = 0; i < 512; i++) {
		/*
		 * Each slot of the level 4 pages points
		 * to the same level 3 page
		 */
		pt4[i] = (uint64_t)(sc->acpi_wakephys + PAGE_SIZE);
		pt4[i] |= PG_V | PG_RW | PG_U;

		/*
		 * Each slot of the level 3 pages points
		 * to the same level 2 page
		 */
		pt3[i] = (uint64_t)(sc->acpi_wakephys + (2 * PAGE_SIZE));
		pt3[i] |= PG_V | PG_RW | PG_U;

		/* The level 2 page slots are mapped with 2MB pages for 1GB. */
		pt2[i] = i * (2 * 1024 * 1024);
		pt2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}
#endif

	if (bootverbose)
		device_printf(sc->acpi_dev, "wakeup code va %#jx pa %#jx\n",
		    (uintmax_t)sc->acpi_wakeaddr, (uintmax_t)sc->acpi_wakephys);
}
