/*-
 * Copyright (c) 2000 Doug Rabson
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
__FBSDID("$FreeBSD: src/sys/alpha/alpha/mp_machdep.c,v 1.56 2005/04/04 21:53:51 jhb Exp $");

#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/cons.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/smp.h>

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready = 0;

static struct mtx ap_boot_mtx;

u_int64_t boot_cpu_id;

static void	release_aps(void *dummy);
static int	smp_cpu_enabled(struct pcs *pcsp);
extern void	smp_init_secondary_glue(void);
static int	smp_send_secondary_command(const char *command, int pal_id);
static int	smp_start_secondary(int pal_id, int cpuid);

/*
 * Communicate with a console running on a secondary processor.
 * Return 1 on failure.
 */
static int
smp_send_secondary_command(const char *command, int pal_id)
{
	u_int64_t mask = 1L << pal_id;
	struct pcs *cpu = LOCATE_PCS(hwrpb, pal_id);
	int i, len;

	/*
	 * Sanity check.
	 */
	len = strlen(command);
	if (len > sizeof(cpu->pcs_buffer.rxbuf)) {
		printf("smp_send_secondary_command: command '%s' too long\n",
		       command);
		return 0;
	}

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	/*
	 * Write the command into the processor's buffer.
	 */
	bcopy(command, cpu->pcs_buffer.rxbuf, len);
	cpu->pcs_buffer.rxlen = len;

	/*
	 * Set the bit in the rxrdy mask and let the secondary try to
	 * handle the command.
	 */
	atomic_set_64(&hwrpb->rpb_rxrdy, mask);

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	return 1;
}

void
smp_init_secondary(void)
{
	struct pcs *cpu;

	/* spin until all the AP's are ready */
	while (!aps_ready)
		/*spin*/ ;
 
	/*
	 * Record the pcpu pointer in the per-cpu system value.
	 */
	alpha_pal_wrval((u_int64_t) pcpup);

	/* Clear userland thread pointer. */
	alpha_pal_wrunique(0);

	/* Initialize curthread. */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT);
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA);
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);


	/* lower the ipl and take any pending machine check */
	mc_expected = 1;
	alpha_mb(); alpha_mb();
	alpha_pal_wrmces(7);
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	mc_expected = 0;

	/*
	 * Set flags in our per-CPU slot in the HWRPB.
	 */
	cpu = LOCATE_PCS(hwrpb, PCPU_GET(pal_id));
	cpu->pcs_flags &= ~PCS_BIP;
	cpu->pcs_flags |= PCS_RC;
	alpha_mb();

	/*
	 * XXX: doesn't idleproc already have a pcb from when it was
	 * kthread_create'd?
	 *
	 * cache idleproc's physical address.
	 */
	curthread->td_md.md_pcbpaddr = (struct pcb *)PCPU_GET(idlepcbphys);
	/*
	 * and make idleproc's trapframe pointer point to its
	 * stack pointer for sanity.
	 */
	curthread->td_frame =
	    (struct trapframe *)PCPU_PTR(idlepcb)->apcb_ksp;

	mtx_lock_spin(&ap_boot_mtx);

	smp_cpus++;

	CTR1(KTR_SMP, "SMP: AP CPU #%d Launched", PCPU_GET(cpuid));

	/* Build our map of 'other' CPUs. */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	if (smp_cpus == mp_ncpus) {
		smp_started = 1;
		smp_active = 1;
	}

	mtx_unlock_spin(&ap_boot_mtx);

	while (smp_started == 0)
		; /* nothing */

	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);

	/*
	 * Correct spinlock nesting.  The idle thread context that we are
	 * borrowing was created so that it would start out with a single
	 * spin lock (sched_lock) held in fork_trampoline().  Since we've
	 * explicitly acquired locks in this function, the nesting count
	 * is now 2 rather than 1.  Since we are nested, calling
	 * spinlock_exit() will simply adjust the counts without allowing
	 * spin lock using code to interrupt us.
	 */
	spinlock_exit();
	KASSERT(curthread->td_md.md_spinlock_count == 1, ("invalid count"));

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	cpu_throw(NULL, choosethread());	/* doesn't return */

	panic("scheduler returned us to %s", __func__);
}

static int
smp_start_secondary(int pal_id, int cpuid)
{
	struct pcs *cpu = LOCATE_PCS(hwrpb, pal_id);
	struct pcs *bootcpu = LOCATE_PCS(hwrpb, boot_cpu_id);
	struct alpha_pcb *pcb = (struct alpha_pcb *) cpu->pcs_hwpcb;
	struct pcpu *pcpu;
	int i;
	size_t sz;

	if ((cpu->pcs_flags & PCS_PV) == 0) {
		printf("smp_start_secondary: cpu %d PALcode invalid\n", pal_id);
		return 0;
	}

	if (bootverbose)
		printf("smp_start_secondary: starting cpu %d\n", pal_id);

	sz = KSTACK_PAGES * PAGE_SIZE;
	pcpu = malloc(sz, M_TEMP, M_NOWAIT);
	if (!pcpu) {
		printf("smp_start_secondary: can't allocate memory\n");
		return 0;
	}
	
	pcpu_init(pcpu, cpuid, sz);
	pcpu->pc_pal_id = pal_id;

	/*
	 * Copy the idle pcb and setup the address to start executing.
	 * Use the pcb unique value to point the secondary at its pcpu
	 * structure.
	 */
	*pcb = pcpu->pc_idlepcb;
	pcb->apcb_unique = (u_int64_t)pcpu;
	hwrpb->rpb_restart = (u_int64_t) smp_init_secondary_glue;
	hwrpb->rpb_restart_val = (u_int64_t) smp_init_secondary_glue;
	hwrpb->rpb_checksum = hwrpb_checksum();

	/*
	 * Tell the cpu to start with the same PALcode as us.
	 */
	bcopy(&bootcpu->pcs_pal_rev, &cpu->pcs_pal_rev,
	      sizeof cpu->pcs_pal_rev);

	/*
	 * Set flags in cpu structure and push out write buffers to
	 * make sure the secondary sees it.
	 */
	cpu->pcs_flags |= PCS_CV|PCS_RC;
	cpu->pcs_flags &= ~PCS_BIP;
	alpha_mb();

	/*
	 * Fire it up and hope for the best.
	 */
	if (!smp_send_secondary_command("START\r\n", pal_id)) {
		printf("smp_start_secondary: can't send START command\n");
		pcpu_destroy(pcpu);
		free(pcpu, M_TEMP);
		return 0;
	}

	/*
	 * Wait for the secondary to set the BIP flag in its structure.
	 */
	for (i = 0; i < 100000; i++) {
		if (cpu->pcs_flags & PCS_BIP)
			break;
		DELAY(10);
	}
	if (!(cpu->pcs_flags & PCS_BIP)) {
		printf("smp_start_secondary: secondary did not respond\n");
		pcpu_destroy(pcpu);
		free(pcpu, M_TEMP);
		return 0;
	}

	/*
	 * It worked (I think).
	 */
	if (bootverbose)
		printf("smp_start_secondary: cpu %d started\n", pal_id);
	return 1;
}

/* Other stuff */

static int
smp_cpu_enabled(struct pcs *pcsp)
{

	/* Is this CPU present? */
	if ((pcsp->pcs_flags & PCS_PP) == 0)
		return (0);

	/* Is this CPU available? */
	if ((pcsp->pcs_flags & PCS_PA) == 0)
		/*
		 * The TurboLaser PCS_PA bit doesn't seem to be set
		 * correctly.
		 */
		if (hwrpb->rpb_type != ST_DEC_21000) 
			return (0);

	/* Is this CPU's PALcode valid? */
	if ((pcsp->pcs_flags & PCS_PV) == 0)
		return (0);

	return (1);
}

void
cpu_mp_setmaxid(void)
{
	u_int64_t i;

	mp_maxid = 0;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (i == PCPU_GET(pal_id))
			continue;
		if (!smp_cpu_enabled(LOCATE_PCS(hwrpb, i)))
			continue;
		mp_maxid++;
	}
	if (mp_maxid > MAXCPU)
		mp_maxid = MAXCPU;
}

int
cpu_mp_probe(void)
{
	int i, cpus;

	/* XXX: Need to check for valid platforms here. */

	boot_cpu_id = PCPU_GET(pal_id);
	KASSERT(boot_cpu_id == hwrpb->rpb_primary_cpu_id,
	    ("cpu_mp_probe() called on non-primary CPU"));
	all_cpus = PCPU_GET(cpumask);

	mp_ncpus = 1;

	/* Make sure we have at least one secondary CPU. */
	cpus = 0;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (i == PCPU_GET(pal_id))
			continue;
		if (!smp_cpu_enabled(LOCATE_PCS(hwrpb, i)))
			continue;
		cpus++;
	}
	return (cpus);
}

void
cpu_mp_start(void)
{
	int i, cpuid;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	cpuid = 1;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		if (i == boot_cpu_id)
			continue;
		pcsp = LOCATE_PCS(hwrpb, i);
		if ((pcsp->pcs_flags & PCS_PP) == 0)
			continue;
		if ((pcsp->pcs_flags & PCS_PA) == 0) {
			if (hwrpb->rpb_type == ST_DEC_21000)  {
				printf("Ignoring PA bit for CPU %d.\n", i);
			} else {
				if (bootverbose)
					printf("CPU %d not available.\n", i);
				continue;
			}
		}
		if ((pcsp->pcs_flags & PCS_PV) == 0) {
			if (bootverbose)
				printf("CPU %d does not have valid PALcode.\n",
				    i);
			continue;
		}
		if (i > MAXCPU) {
			if (bootverbose) {
				printf("CPU %d not supported.", i);
				printf("  Only %d CPUs supported.\n", MAXCPU);
			}
			continue;
		}
		if (resource_disabled("cpu", i)) {
			printf("CPU %d disabled by loader.\n", i);
			continue;
		}
		if (smp_start_secondary(i, cpuid)) {
			all_cpus |= (1 << cpuid);
			mp_ncpus++;
			cpuid++;
		}
	}
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));
}

void
cpu_mp_announce(void)
{
	struct pcpu *pc;
	int i;
	
	/* List CPUs */
	printf(" cpu0 (BSP): PAL ID: %2lu\n", boot_cpu_id);
	for (i = 1; i < MAXCPU; i++) {
		if (CPU_ABSENT(i))
			continue;
		pc = pcpu_find(i);
		MPASS(pc != NULL);
		printf(" cpu%d (AP): PAL ID: %2lu\n", i, pc->pc_pal_id);
	}
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int64_t ipi)
{
	struct pcpu *pcpu;

	CTR2(KTR_SMP, "ipi_selected: cpus: %x ipi: %lx", cpus, ipi);
	alpha_mb();
	while (cpus) {
		int cpuid = ffs(cpus) - 1;
		cpus &= ~(1 << cpuid);

		pcpu = pcpu_find(cpuid);
		if (pcpu) {
			atomic_set_64(&pcpu->pc_pending_ipis, ipi);
			alpha_mb();
			CTR1(KTR_SMP, "calling alpha_pal_wripir(%d)",
			    pcpu->pc_pal_id);
			alpha_pal_wripir(pcpu->pc_pal_id);
		}
	}
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
void
ipi_all(u_int64_t ipi)
{
	ipi_selected(all_cpus, ipi);
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int64_t ipi)
{
	ipi_selected(PCPU_GET(other_cpus), ipi);
}

/*
 * send an IPI to myself
 */
void
ipi_self(u_int64_t ipi)
{
	ipi_selected(PCPU_GET(cpumask), ipi);
}

/*
 * Handle an IPI sent to this processor.
 */
void
smp_handle_ipi(struct trapframe *frame)
{
	u_int64_t ipis = atomic_readandclear_64(PCPU_PTR(pending_ipis));
	u_int64_t ipi;
	int cpumask;

	cpumask = PCPU_GET(cpumask);

	CTR1(KTR_SMP, "smp_handle_ipi(), ipis=%lx", ipis);
	while (ipis) {
		/*
		 * Find the lowest set bit.
		 */
		ipi = ipis & ~(ipis - 1);
		ipis &= ~ipi;
		switch (ipi) {
		case IPI_INVLTLB:
			CTR0(KTR_SMP, "IPI_NVLTLB");
			ALPHA_TBIA();
			break;

		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;

		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			break;

		case IPI_STOP:
			CTR0(KTR_SMP, "IPI_STOP");
			atomic_set_int(&stopped_cpus, cpumask);
			while ((started_cpus & cpumask) == 0)
				alpha_mb();
			atomic_clear_int(&started_cpus, cpumask);
			atomic_clear_int(&stopped_cpus, cpumask);
			break;
		}
	}

	/*
	 * Dump console messages to the console.  XXX - we need to handle
	 * requests to provide PALcode to secondaries and to start up new
	 * secondaries that are added to the system on the fly.
	 */
	if (PCPU_GET(pal_id) == boot_cpu_id) {
		u_int pal_id;
		u_int64_t txrdy;
#ifdef DIAGNOSTIC
		struct pcs *cpu;
		char buf[81];
#endif

		alpha_mb();
		while (hwrpb->rpb_txrdy != 0) {
			pal_id = ffs(hwrpb->rpb_txrdy) - 1;
#ifdef DIAGNOSTIC
			cpu = LOCATE_PCS(hwrpb, pal_id);
			bcopy(&cpu->pcs_buffer.txbuf, buf,
			    cpu->pcs_buffer.txlen);
			buf[cpu->pcs_buffer.txlen] = '\0';
			printf("SMP From CPU%d: %s\n", pal_id, buf);
#endif
			do {
				txrdy = hwrpb->rpb_txrdy;
			} while (atomic_cmpset_64(&hwrpb->rpb_txrdy, txrdy,
			    txrdy & ~(1 << pal_id)) == 0);
		}
	}
}

static void
release_aps(void *dummy __unused)
{
	if (bootverbose && mp_ncpus > 1)
		printf("%s: releasing secondary CPUs\n", __func__);
	atomic_store_rel_int(&aps_ready, 1);

	while (mp_ncpus > 1 && smp_started == 0)
		; /* nothing */
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);
