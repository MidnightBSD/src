/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
__FBSDID("$FreeBSD$");

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
extern void		**suspfpusave;
#else
static struct pcb	**susppcbs;
static void		**suspfpusave;
#endif

int			acpi_restorecpu(uint64_t, vm_offset_t);

static void		*acpi_alloc_wakeup_handler(void);
static void		acpi_stop_beep(void *);

#ifdef SMP
static int		acpi_wakeup_ap(struct acpi_softc *, int);
static void		acpi_wakeup_cpus(struct acpi_softc *, const cpuset_t *);
#endif

#define	WAKECODE_VADDR(sc)	((sc)->acpi_wakeaddr + (3 * PAGE_SIZE))
#define	WAKECODE_PADDR(sc)	((sc)->acpi_wakephys + (3 * PAGE_SIZE))
#define	WAKECODE_FIXUP(offset, type, val) do	{	\
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
	WAKECODE_FIXUP(wakeup_fpusave, void *, suspfpusave[cpu]);
	WAKECODE_FIXUP(wakeup_gdt, uint16_t, susppcbs[cpu]->pcb_gdt.rd_limit);
	WAKECODE_FIXUP(wakeup_gdt + 2, uint64_t,
	    susppcbs[cpu]->pcb_gdt.rd_base);
	WAKECODE_FIXUP(wakeup_cpu, int, cpu);

	/* do an INIT IPI: assert RESET */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_ASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_INIT, apic_id);

	/* wait for pending status end */
	lapic_ipi_wait(-1);

	/* do an INIT IPI: deassert RESET */
	lapic_ipi_raw(APIC_DEST_ALLESELF | APIC_TRIGMOD_LEVEL |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_INIT, 0);

	/* wait for pending status end */
	DELAY(10000);		/* wait ~10mS */
	lapic_ipi_wait(-1);

	/*
	 * next we do a STARTUP IPI: the previous INIT IPI might still be
	 * latched, (P5 bug) this 1st STARTUP would then terminate
	 * immediately, and the previously started INIT IPI would continue. OR
	 * the previous INIT IPI has already run. and this STARTUP IPI will
	 * run. OR the previous INIT IPI was ignored. and this STARTUP IPI
	 * will run.
	 */

	/* do a STARTUP IPI */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_STARTUP |
	    vector, apic_id);
	lapic_ipi_wait(-1);
	DELAY(200);		/* wait ~200uS */

	/*
	 * finally we do a 2nd STARTUP IPI: this 2nd STARTUP IPI should run IF
	 * the previous STARTUP IPI was cancelled by a latched INIT IPI. OR
	 * this STARTUP IPI will be ignored, as only ONE STARTUP IPI is
	 * recognized after hardware RESET or INIT IPI.
	 */

	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_STARTUP |
	    vector, apic_id);
	lapic_ipi_wait(-1);
	DELAY(200);		/* wait ~200uS */

	/* Wait up to 5 seconds for it to start. */
	for (ms = 0; ms < 5000; ms++) {
		if (*(int *)(WAKECODE_VADDR(sc) + wakeup_cpu) == 0)
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
acpi_wakeup_cpus(struct acpi_softc *sc, const cpuset_t *wakeup_cpus)
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
		if (!CPU_ISSET(cpu, wakeup_cpus))
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
#ifdef SMP
	cpuset_t	wakeup_cpus;
#endif
	register_t	rf;
	ACPI_STATUS	status;
	int		ret;

	ret = -1;

	if (sc->acpi_wakeaddr == 0ul)
		return (ret);

#ifdef SMP
	wakeup_cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &wakeup_cpus);
#endif

	if (acpi_resume_beep != 0)
		timer_spkr_acquire();

	AcpiSetFirmwareWakingVector(WAKECODE_PADDR(sc));

	rf = intr_disable();
	intr_suspend();

	if (savectx(susppcbs[0])) {
		ctx_fpusave(suspfpusave[0]);
#ifdef SMP
		if (!CPU_EMPTY(&wakeup_cpus) &&
		    suspend_cpus(wakeup_cpus) == 0) {
			device_printf(sc->acpi_dev, "Failed to suspend APs\n");
			goto out;
		}
#endif

		WAKECODE_FIXUP(resume_beep, uint8_t, (acpi_resume_beep != 0));
		WAKECODE_FIXUP(reset_video, uint8_t, (acpi_reset_video != 0));

		WAKECODE_FIXUP(wakeup_pcb, struct pcb *, susppcbs[0]);
		WAKECODE_FIXUP(wakeup_fpusave, void *, suspfpusave[0]);
		WAKECODE_FIXUP(wakeup_gdt, uint16_t,
		    susppcbs[0]->pcb_gdt.rd_limit);
		WAKECODE_FIXUP(wakeup_gdt + 2, uint64_t,
		    susppcbs[0]->pcb_gdt.rd_base);
		WAKECODE_FIXUP(wakeup_cpu, int, 0);

		/* Call ACPICA to enter the desired sleep state */
		if (state == ACPI_STATE_S4 && sc->acpi_s4bios)
			status = AcpiEnterSleepStateS4bios();
		else
			status = AcpiEnterSleepState(state);

		if (status != AE_OK) {
			device_printf(sc->acpi_dev,
			    "AcpiEnterSleepState failed - %s\n",
			    AcpiFormatException(status));
			goto out;
		}

		for (;;)
			ia32_pause();
	} else {
		pmap_init_pat();
		load_cr3(susppcbs[0]->pcb_cr3);
		initializecpu();
		PCPU_SET(switchtime, 0);
		PCPU_SET(switchticks, ticks);
#ifdef SMP
		if (!CPU_EMPTY(&wakeup_cpus))
			acpi_wakeup_cpus(sc, &wakeup_cpus);
#endif
		ret = 0;
	}

out:
#ifdef SMP
	if (!CPU_EMPTY(&wakeup_cpus))
		restart_cpus(wakeup_cpus);
#endif

	mca_resume();
	intr_resume();
	intr_restore(rf);

	AcpiSetFirmwareWakingVector(0);

	if (ret == 0 && mem_range_softc.mr_op != NULL &&
	    mem_range_softc.mr_op->reinit != NULL)
		mem_range_softc.mr_op->reinit(&mem_range_softc);

	return (ret);
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
	wakeaddr = contigmalloc(4 * PAGE_SIZE, M_DEVBUF, M_WAITOK, 0x500,
	    0xa0000, PAGE_SIZE, 0ul);
	if (wakeaddr == NULL) {
		printf("%s: can't alloc wake memory\n", __func__);
		return (NULL);
	}
	if (EVENTHANDLER_REGISTER(power_resume, acpi_stop_beep, NULL,
	    EVENTHANDLER_PRI_LAST) == NULL) {
		printf("%s: can't register event handler\n", __func__);
		contigfree(wakeaddr, 4 * PAGE_SIZE, M_DEVBUF);
		return (NULL);
	}
	susppcbs = malloc(mp_ncpus * sizeof(*susppcbs), M_DEVBUF, M_WAITOK);
	suspfpusave = malloc(mp_ncpus * sizeof(void *), M_DEVBUF, M_WAITOK);
	for (i = 0; i < mp_ncpus; i++) {
		susppcbs[i] = malloc(sizeof(**susppcbs), M_DEVBUF, M_WAITOK);
		suspfpusave[i] = alloc_fpusave(M_WAITOK);
	}

	return (wakeaddr);
}

void
acpi_install_wakeup_handler(struct acpi_softc *sc)
{
	static void	*wakeaddr = NULL;
	uint64_t	*pt4, *pt3, *pt2;
	int		i;

	if (wakeaddr != NULL)
		return;

	wakeaddr = acpi_alloc_wakeup_handler();
	if (wakeaddr == NULL)
		return;

	sc->acpi_wakeaddr = (vm_offset_t)wakeaddr;
	sc->acpi_wakephys = vtophys(wakeaddr);

	bcopy(wakecode, (void *)WAKECODE_VADDR(sc), sizeof(wakecode));

	/* Patch GDT base address, ljmp targets and page table base address. */
	WAKECODE_FIXUP((bootgdtdesc + 2), uint32_t,
	    WAKECODE_PADDR(sc) + bootgdt);
	WAKECODE_FIXUP((wakeup_sw32 + 2), uint32_t,
	    WAKECODE_PADDR(sc) + wakeup_32);
	WAKECODE_FIXUP((wakeup_sw64 + 1), uint32_t,
	    WAKECODE_PADDR(sc) + wakeup_64);
	WAKECODE_FIXUP(wakeup_pagetables, uint32_t, sc->acpi_wakephys);

	/* Save pointers to some global data. */
	WAKECODE_FIXUP(wakeup_retaddr, void *, acpi_restorecpu);
	WAKECODE_FIXUP(wakeup_kpml4, uint64_t, KPML4phys);
	WAKECODE_FIXUP(wakeup_ctx, vm_offset_t,
	    WAKECODE_VADDR(sc) + wakeup_ctx);
	WAKECODE_FIXUP(wakeup_efer, uint64_t, rdmsr(MSR_EFER));
	WAKECODE_FIXUP(wakeup_star, uint64_t, rdmsr(MSR_STAR));
	WAKECODE_FIXUP(wakeup_lstar, uint64_t, rdmsr(MSR_LSTAR));
	WAKECODE_FIXUP(wakeup_cstar, uint64_t, rdmsr(MSR_CSTAR));
	WAKECODE_FIXUP(wakeup_sfmask, uint64_t, rdmsr(MSR_SF_MASK));
	WAKECODE_FIXUP(wakeup_xsmask, uint64_t, xsave_mask);

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

	if (bootverbose)
		device_printf(sc->acpi_dev, "wakeup code va %p pa %p\n",
		    (void *)sc->acpi_wakeaddr, (void *)sc->acpi_wakephys);
}
