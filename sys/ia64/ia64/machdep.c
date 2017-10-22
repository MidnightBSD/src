/*-
 * Copyright (c) 2003,2004 Marcel Moolenaar
 * Copyright (c) 2000,2001 Doug Rabson
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

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/random.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/uuid.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <ddb/ddb.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/bootinfo.h>
#include <machine/cpu.h>
#include <machine/efi.h>
#include <machine/elf.h>
#include <machine/fpu.h>
#include <machine/intr.h>
#include <machine/mca.h>
#include <machine/md_var.h>
#include <machine/pal.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/sal.h>
#include <machine/sigframe.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/unwind.h>
#include <machine/vmparam.h>

SYSCTL_NODE(_hw, OID_AUTO, freq, CTLFLAG_RD, 0, "");
SYSCTL_NODE(_machdep, OID_AUTO, cpu, CTLFLAG_RD, 0, "");

static u_int bus_freq;
SYSCTL_UINT(_hw_freq, OID_AUTO, bus, CTLFLAG_RD, &bus_freq, 0,
    "Bus clock frequency");

static u_int cpu_freq;
SYSCTL_UINT(_hw_freq, OID_AUTO, cpu, CTLFLAG_RD, &cpu_freq, 0,
    "CPU clock frequency");

static u_int itc_freq;
SYSCTL_UINT(_hw_freq, OID_AUTO, itc, CTLFLAG_RD, &itc_freq, 0,
    "ITC frequency");

int cold = 1;

struct bootinfo *bootinfo;

struct pcpu pcpu0;

extern u_int64_t kernel_text[], _end[];

extern u_int64_t ia64_gateway_page[];
extern u_int64_t break_sigtramp[];
extern u_int64_t epc_sigtramp[];

struct fpswa_iface *fpswa_iface;

vm_size_t ia64_pal_size;
vm_paddr_t ia64_pal_base;
vm_offset_t ia64_port_base;

u_int64_t ia64_lapic_addr = PAL_PIB_DEFAULT_ADDR;

struct ia64_pib *ia64_pib;

static int ia64_sync_icache_needed;

char machine[] = MACHINE;
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static char cpu_model[64];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, cpu_model, 0,
    "The CPU model name");

static char cpu_family[64];
SYSCTL_STRING(_hw, OID_AUTO, family, CTLFLAG_RD, cpu_family, 0,
    "The CPU family name");

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif


struct msgbuf *msgbufp = NULL;

/* Other subsystems (e.g., ACPI) can hook this later. */
void (*cpu_idle_hook)(void) = NULL;

long Maxmem = 0;
long realmem = 0;

#define	PHYSMAP_SIZE	(2 * VM_PHYSSEG_MAX)

vm_paddr_t phys_avail[PHYSMAP_SIZE + 2];

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(vm_offset_t)) - 2)

struct kva_md_info kmi;

#define	Mhz	1000000L
#define	Ghz	(1000L*Mhz)

static void
identifycpu(void)
{
	char vendor[17];
	char *family_name, *model_name;
	u_int64_t features, tmp;
	int number, revision, model, family, archrev;

	/*
	 * Assumes little-endian.
	 */
	*(u_int64_t *) &vendor[0] = ia64_get_cpuid(0);
	*(u_int64_t *) &vendor[8] = ia64_get_cpuid(1);
	vendor[16] = '\0';

	tmp = ia64_get_cpuid(3);
	number = (tmp >> 0) & 0xff;
	revision = (tmp >> 8) & 0xff;
	model = (tmp >> 16) & 0xff;
	family = (tmp >> 24) & 0xff;
	archrev = (tmp >> 32) & 0xff;

	family_name = model_name = "unknown";
	switch (family) {
	case 0x07:
		family_name = "Itanium";
		model_name = "Merced";
		break;
	case 0x1f:
		family_name = "Itanium 2";
		switch (model) {
		case 0x00:
			model_name = "McKinley";
			break;
		case 0x01:
			/*
			 * Deerfield is a low-voltage variant based on the
			 * Madison core. We need circumstantial evidence
			 * (i.e. the clock frequency) to identify those.
			 * Allow for roughly 1% error margin.
			 */
			if (cpu_freq > 990 && cpu_freq < 1010)
				model_name = "Deerfield";
			else
				model_name = "Madison";
			break;
		case 0x02:
			model_name = "Madison II";
			break;
		}
		break;
	case 0x20:
		ia64_sync_icache_needed = 1;

		family_name = "Itanium 2";
		switch (model) {
		case 0x00:
			model_name = "Montecito";
			break;
		case 0x01:
			model_name = "Montvale";
			break;
		}
		break;
	}
	snprintf(cpu_family, sizeof(cpu_family), "%s", family_name);
	snprintf(cpu_model, sizeof(cpu_model), "%s", model_name);

	features = ia64_get_cpuid(4);

	printf("CPU: %s (", model_name);
	if (cpu_freq)
		printf("%u MHz ", cpu_freq);
	printf("%s)\n", family_name);
	printf("  Origin = \"%s\"  Revision = %d\n", vendor, revision);
	printf("  Features = 0x%b\n", (u_int32_t) features,
	    "\020"
	    "\001LB"	/* long branch (brl) instruction. */
	    "\002SD"	/* Spontaneous deferral. */
	    "\003AO"	/* 16-byte atomic operations (ld, st, cmpxchg). */ );
}

static void
cpu_startup(void *dummy)
{
	char nodename[16];
	struct pcpu *pc;
	struct pcpu_stats *pcs;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	identifycpu();

#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ld (%ld MB)\n", ia64_ptob(Maxmem),
	    ia64_ptob(Maxmem) / 1048576);
	realmem = Maxmem;

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			long size1 = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08lx - 0x%08lx, %ld bytes (%ld pages)\n",
			    phys_avail[indx], phys_avail[indx + 1] - 1, size1,
			    size1 >> PAGE_SHIFT);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ld (%ld MB)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1048576);
 
	if (fpswa_iface == NULL)
		printf("Warning: no FPSWA package supplied\n");
	else
		printf("FPSWA Revision = 0x%lx, Entry = %p\n",
		    (long)fpswa_iface->if_rev, (void *)fpswa_iface->if_fpswa);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

	/*
	 * Traverse the MADT to discover IOSAPIC and Local SAPIC
	 * information.
	 */
	ia64_probe_sapics();
	ia64_pib = pmap_mapdev(ia64_lapic_addr, sizeof(*ia64_pib));

	ia64_mca_init();

	/*
	 * Create sysctl tree for per-CPU information.
	 */
	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		snprintf(nodename, sizeof(nodename), "%u", pc->pc_cpuid);
		sysctl_ctx_init(&pc->pc_md.sysctl_ctx);
		pc->pc_md.sysctl_tree = SYSCTL_ADD_NODE(&pc->pc_md.sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_machdep_cpu), OID_AUTO, nodename,
		    CTLFLAG_RD, NULL, "");
		if (pc->pc_md.sysctl_tree == NULL)
			continue;

		pcs = &pc->pc_md.stats;

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nasts", CTLFLAG_RD, &pcs->pcs_nasts,
		    "Number of IPI_AST interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nclks", CTLFLAG_RD, &pcs->pcs_nclks,
		    "Number of clock interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nextints", CTLFLAG_RD, &pcs->pcs_nextints,
		    "Number of ExtINT interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nhardclocks", CTLFLAG_RD, &pcs->pcs_nhardclocks,
		    "Number of IPI_HARDCLOCK interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nhighfps", CTLFLAG_RD, &pcs->pcs_nhighfps,
		    "Number of IPI_HIGH_FP interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nhwints", CTLFLAG_RD, &pcs->pcs_nhwints,
		    "Number of hardware (device) interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "npreempts", CTLFLAG_RD, &pcs->pcs_npreempts,
		    "Number of IPI_PREEMPT interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nrdvs", CTLFLAG_RD, &pcs->pcs_nrdvs,
		    "Number of IPI_RENDEZVOUS interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nstops", CTLFLAG_RD, &pcs->pcs_nstops,
		    "Number of IPI_STOP interrupts");

		SYSCTL_ADD_ULONG(&pc->pc_md.sysctl_ctx,
		    SYSCTL_CHILDREN(pc->pc_md.sysctl_tree), OID_AUTO,
		    "nstrays", CTLFLAG_RD, &pcs->pcs_nstrays,
		    "Number of stray interrupts");
	}
}
SYSINIT(cpu_startup, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

void
cpu_flush_dcache(void *ptr, size_t len)
{
	vm_offset_t lim, va;

	va = (uintptr_t)ptr & ~31;
	lim = (uintptr_t)ptr + len;
	while (va < lim) {
		ia64_fc(va);
		va += 32;
	}

	ia64_srlz_d();
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	if (pcpu_find(cpu_id) == NULL || rate == NULL)
		return (EINVAL);
	*rate = (u_long)cpu_freq * 1000000ul;
	return (0);
}

void
cpu_halt()
{

	efi_reset_system();
}

void
cpu_idle(int busy)
{
	register_t ie;

	if (!busy) {
		critical_enter();
		cpu_idleclock();
	}

	ie = intr_disable();
	KASSERT(ie != 0, ("%s called with interrupts disabled\n", __func__));

	if (sched_runnable())
		ia64_enable_intr();
	else if (cpu_idle_hook != NULL) {
		(*cpu_idle_hook)();
		/* The hook must enable interrupts! */
	} else {
		ia64_call_pal_static(PAL_HALT_LIGHT, 0, 0, 0);
		ia64_enable_intr();
	}

	if (!busy) {
		cpu_activeclock();
		critical_exit();
	}
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
}

void
cpu_reset()
{

	efi_reset_system();
}

void
cpu_switch(struct thread *old, struct thread *new, struct mtx *mtx)
{
	struct pcb *oldpcb, *newpcb;

	oldpcb = old->td_pcb;
#ifdef COMPAT_FREEBSD32
	ia32_savectx(oldpcb);
#endif
	if (PCPU_GET(fpcurthread) == old)
		old->td_frame->tf_special.psr |= IA64_PSR_DFH;
	if (!savectx(oldpcb)) {
		newpcb = new->td_pcb;
		oldpcb->pcb_current_pmap =
		    pmap_switch(newpcb->pcb_current_pmap);

		atomic_store_rel_ptr(&old->td_lock, mtx);

#if defined(SCHED_ULE) && defined(SMP)
		while (atomic_load_acq_ptr(&new->td_lock) == &blocked_lock)
			cpu_spinwait();
#endif

		PCPU_SET(curthread, new);

#ifdef COMPAT_FREEBSD32
		ia32_restorectx(newpcb);
#endif

		if (PCPU_GET(fpcurthread) == new)
			new->td_frame->tf_special.psr &= ~IA64_PSR_DFH;
		restorectx(newpcb);
		/* We should not get here. */
		panic("cpu_switch: restorectx() returned");
		/* NOTREACHED */
	}
}

void
cpu_throw(struct thread *old __unused, struct thread *new)
{
	struct pcb *newpcb;

	newpcb = new->td_pcb;
	(void)pmap_switch(newpcb->pcb_current_pmap);

#if defined(SCHED_ULE) && defined(SMP)
	while (atomic_load_acq_ptr(&new->td_lock) == &blocked_lock)
		cpu_spinwait();
#endif

	PCPU_SET(curthread, new);

#ifdef COMPAT_FREEBSD32
	ia32_restorectx(newpcb);
#endif

	restorectx(newpcb);
	/* We should not get here. */
	panic("cpu_throw: restorectx() returned");
	/* NOTREACHED */
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	/*
	 * Set pc_acpi_id to "uninitialized".
	 * See sys/dev/acpica/acpi_cpu.c
	 */
	pcpu->pc_acpi_id = 0xffffffff;
}

void
spinlock_enter(void)
{
	struct thread *td;
	int intr;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		intr = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_intr = intr;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	int intr;

	td = curthread;
	critical_exit();
	intr = td->td_md.md_saved_intr;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		intr_restore(intr);
}

void
map_vhpt(uintptr_t vhpt)
{
	pt_entry_t pte;
	uint64_t psr;

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | PTE_AR_RW;
	pte |= vhpt & PTE_PPN_MASK;

	__asm __volatile("ptr.d %0,%1" :: "r"(vhpt),
	    "r"(pmap_vhpt_log2size << 2));

	__asm __volatile("mov   %0=psr" : "=r"(psr));
	__asm __volatile("rsm   psr.ic|psr.i");
	ia64_srlz_i();
	ia64_set_ifa(vhpt);
	ia64_set_itir(pmap_vhpt_log2size << 2);
	ia64_srlz_d();
	__asm __volatile("itr.d dtr[%0]=%1" :: "r"(3), "r"(pte));
	__asm __volatile("mov   psr.l=%0" :: "r" (psr));
	ia64_srlz_i();
}

void
map_pal_code(void)
{
	pt_entry_t pte;
	vm_offset_t va;
	vm_size_t sz;
	uint64_t psr;
	u_int shft;

	if (ia64_pal_size == 0)
		return;

	va = IA64_PHYS_TO_RR7(ia64_pal_base);

	sz = ia64_pal_size;
	shft = 0;
	while (sz > 1) {
		shft++;
		sz >>= 1;
	}

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | PTE_AR_RWX;
	pte |= ia64_pal_base & PTE_PPN_MASK;

	__asm __volatile("ptr.d %0,%1; ptr.i %0,%1" :: "r"(va), "r"(shft<<2));

	__asm __volatile("mov	%0=psr" : "=r"(psr));
	__asm __volatile("rsm	psr.ic|psr.i");
	ia64_srlz_i();
	ia64_set_ifa(va);
	ia64_set_itir(shft << 2);
	ia64_srlz_d();
	__asm __volatile("itr.d	dtr[%0]=%1" :: "r"(4), "r"(pte));
	ia64_srlz_d();
	__asm __volatile("itr.i	itr[%0]=%1" :: "r"(1), "r"(pte));
	__asm __volatile("mov	psr.l=%0" :: "r" (psr));
	ia64_srlz_i();
}

void
map_gateway_page(void)
{
	pt_entry_t pte;
	uint64_t psr;

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | PTE_AR_X_RX;
	pte |= ia64_tpa((uint64_t)ia64_gateway_page) & PTE_PPN_MASK;

	__asm __volatile("ptr.d %0,%1; ptr.i %0,%1" ::
	    "r"(VM_MAXUSER_ADDRESS), "r"(PAGE_SHIFT << 2));

	__asm __volatile("mov	%0=psr" : "=r"(psr));
	__asm __volatile("rsm	psr.ic|psr.i");
	ia64_srlz_i();
	ia64_set_ifa(VM_MAXUSER_ADDRESS);
	ia64_set_itir(PAGE_SHIFT << 2);
	ia64_srlz_d();
	__asm __volatile("itr.d	dtr[%0]=%1" :: "r"(5), "r"(pte));
	ia64_srlz_d();
	__asm __volatile("itr.i	itr[%0]=%1" :: "r"(2), "r"(pte));
	__asm __volatile("mov	psr.l=%0" :: "r" (psr));
	ia64_srlz_i();

	/* Expose the mapping to userland in ar.k5 */
	ia64_set_k5(VM_MAXUSER_ADDRESS);
}

static u_int
freq_ratio(u_long base, u_long ratio)
{
	u_long f;

	f = (base * (ratio >> 32)) / (ratio & 0xfffffffful);
	return ((f + 500000) / 1000000);
}

static void
calculate_frequencies(void)
{
	struct ia64_sal_result sal;
	struct ia64_pal_result pal;
	register_t ie;

	ie = intr_disable();
	sal = ia64_sal_entry(SAL_FREQ_BASE, 0, 0, 0, 0, 0, 0, 0);
	pal = ia64_call_pal_static(PAL_FREQ_RATIOS, 0, 0, 0);
	intr_restore(ie);

	if (sal.sal_status == 0 && pal.pal_status == 0) {
		if (bootverbose) {
			printf("Platform clock frequency %ld Hz\n",
			       sal.sal_result[0]);
			printf("Processor ratio %ld/%ld, Bus ratio %ld/%ld, "
			       "ITC ratio %ld/%ld\n",
			       pal.pal_result[0] >> 32,
			       pal.pal_result[0] & ((1L << 32) - 1),
			       pal.pal_result[1] >> 32,
			       pal.pal_result[1] & ((1L << 32) - 1),
			       pal.pal_result[2] >> 32,
			       pal.pal_result[2] & ((1L << 32) - 1));
		}
		cpu_freq = freq_ratio(sal.sal_result[0], pal.pal_result[0]);
		bus_freq = freq_ratio(sal.sal_result[0], pal.pal_result[1]);
		itc_freq = freq_ratio(sal.sal_result[0], pal.pal_result[2]);
	}
}

struct ia64_init_return
ia64_init(void)
{
	struct ia64_init_return ret;
	int phys_avail_cnt;
	vm_offset_t kernstart, kernend;
	vm_offset_t kernstartpfn, kernendpfn, pfn0, pfn1;
	char *p;
	struct efi_md *md;
	int metadata_missing;

	/* NO OUTPUT ALLOWED UNTIL FURTHER NOTICE */

	/*
	 * TODO: Disable interrupts, floating point etc.
	 * Maybe flush cache and tlb
	 */
	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	/*
	 * TODO: Get critical system information (if possible, from the
	 * information provided by the boot program).
	 */

	/*
	 * Look for the I/O ports first - we need them for console
	 * probing.
	 */
	for (md = efi_md_first(); md != NULL; md = efi_md_next(md)) {
		switch (md->md_type) {
		case EFI_MD_TYPE_IOPORT:
			ia64_port_base = (uintptr_t)pmap_mapdev(md->md_phys,
			    md->md_pages * EFI_PAGE_SIZE);
			break;
		case EFI_MD_TYPE_PALCODE:
			ia64_pal_size = md->md_pages * EFI_PAGE_SIZE;
			ia64_pal_base = md->md_phys;
			break;
		}
	}

	metadata_missing = 0;
	if (bootinfo->bi_modulep)
		preload_metadata = (caddr_t)bootinfo->bi_modulep;
	else
		metadata_missing = 1;

	if (envmode == 0 && bootinfo->bi_envp)
		kern_envp = (caddr_t)bootinfo->bi_envp;
	else
		kern_envp = static_env;

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = bootinfo->bi_boothowto;

	if (boothowto & RB_VERBOSE)
		bootverbose = 1;

	/*
	 * Find the beginning and end of the kernel.
	 */
	kernstart = trunc_page(kernel_text);
#ifdef DDB
	ksym_start = bootinfo->bi_symtab;
	ksym_end = bootinfo->bi_esymtab;
	kernend = (vm_offset_t)round_page(ksym_end);
#else
	kernend = (vm_offset_t)round_page(_end);
#endif
	/* But if the bootstrap tells us otherwise, believe it! */
	if (bootinfo->bi_kernend)
		kernend = round_page(bootinfo->bi_kernend);

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (PAGE_SHIFT << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (PAGE_SHIFT << 2));
	ia64_srlz_d();

	/*
	 * Wire things up so we can call the firmware.
	 */
	map_pal_code();
	efi_boot_minimal(bootinfo->bi_systab);
	ia64_xiv_init();
	ia64_sal_init();
	calculate_frequencies();

	set_cputicker(ia64_get_itc, (u_long)itc_freq * 1000000, 0);

	/*
	 * Setup the PCPU data for the bootstrap processor. It is needed
	 * by printf(). Also, since printf() has critical sections, we
	 * need to initialize at least pc_curthread.
	 */
	pcpup = &pcpu0;
	ia64_set_k4((u_int64_t)pcpup);
	pcpu_init(pcpup, 0, sizeof(pcpu0));
	dpcpu_init((void *)kernend, 0);
	PCPU_SET(md.lid, ia64_get_lid());
	kernend += DPCPU_SIZE;
	PCPU_SET(curthread, &thread0);

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

	/* OUTPUT NOW ALLOWED */

	if (metadata_missing)
		printf("WARNING: loader(8) metadata is missing!\n");

	/* Get FPSWA interface */
	fpswa_iface = (bootinfo->bi_fpswa == 0) ? NULL :
	    (struct fpswa_iface *)IA64_PHYS_TO_RR7(bootinfo->bi_fpswa);

	/* Init basic tunables, including hz */
	init_param1();

	p = getenv("kernelname");
	if (p != NULL) {
		strlcpy(kernelname, p, sizeof(kernelname));
		freeenv(p);
	}

	kernstartpfn = atop(IA64_RR_MASK(kernstart));
	kernendpfn = atop(IA64_RR_MASK(kernend));

	/*
	 * Size the memory regions and load phys_avail[] with the results.
	 */

	/*
	 * Find out how much memory is available, by looking at
	 * the memory descriptors.
	 */

#ifdef DEBUG_MD
	printf("Memory descriptor count: %d\n", mdcount);
#endif

	phys_avail_cnt = 0;
	for (md = efi_md_first(); md != NULL; md = efi_md_next(md)) {
#ifdef DEBUG_MD
		printf("MD %p: type %d pa 0x%lx cnt 0x%lx\n", md,
		    md->md_type, md->md_phys, md->md_pages);
#endif

		pfn0 = ia64_btop(round_page(md->md_phys));
		pfn1 = ia64_btop(trunc_page(md->md_phys + md->md_pages * 4096));
		if (pfn1 <= pfn0)
			continue;

		if (md->md_type != EFI_MD_TYPE_FREE)
			continue;

		/*
		 * We have a memory descriptor that describes conventional
		 * memory that is for general use. We must determine if the
		 * loader has put the kernel in this region.
		 */
		physmem += (pfn1 - pfn0);
		if (pfn0 <= kernendpfn && kernstartpfn <= pfn1) {
			/*
			 * Must compute the location of the kernel
			 * within the segment.
			 */
#ifdef DEBUG_MD
			printf("Descriptor %p contains kernel\n", mp);
#endif
			if (pfn0 < kernstartpfn) {
				/*
				 * There is a chunk before the kernel.
				 */
#ifdef DEBUG_MD
				printf("Loading chunk before kernel: "
				       "0x%lx / 0x%lx\n", pfn0, kernstartpfn);
#endif
				phys_avail[phys_avail_cnt] = ia64_ptob(pfn0);
				phys_avail[phys_avail_cnt+1] = ia64_ptob(kernstartpfn);
				phys_avail_cnt += 2;
			}
			if (kernendpfn < pfn1) {
				/*
				 * There is a chunk after the kernel.
				 */
#ifdef DEBUG_MD
				printf("Loading chunk after kernel: "
				       "0x%lx / 0x%lx\n", kernendpfn, pfn1);
#endif
				phys_avail[phys_avail_cnt] = ia64_ptob(kernendpfn);
				phys_avail[phys_avail_cnt+1] = ia64_ptob(pfn1);
				phys_avail_cnt += 2;
			}
		} else {
			/*
			 * Just load this cluster as one chunk.
			 */
#ifdef DEBUG_MD
			printf("Loading descriptor %d: 0x%lx / 0x%lx\n", i,
			       pfn0, pfn1);
#endif
			phys_avail[phys_avail_cnt] = ia64_ptob(pfn0);
			phys_avail[phys_avail_cnt+1] = ia64_ptob(pfn1);
			phys_avail_cnt += 2;
			
		}
	}
	phys_avail[phys_avail_cnt] = 0;

	Maxmem = physmem;
	init_param2(physmem);

	/*
	 * Initialize error message buffer (at end of core).
	 */
	msgbufp = (struct msgbuf *)pmap_steal_memory(msgbufsize);
	msgbufinit(msgbufp, msgbufsize);

	proc_linkup0(&proc0, &thread0);
	/*
	 * Init mapping for kernel stack for proc 0
	 */
	thread0.td_kstack = pmap_steal_memory(KSTACK_PAGES * PAGE_SIZE);
	thread0.td_kstack_pages = KSTACK_PAGES;

	mutex_init();

	/*
	 * Initialize the rest of proc 0's PCB.
	 *
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 * Initialise proc0's backing store to start after u area.
	 */
	cpu_thread_alloc(&thread0);
	thread0.td_frame->tf_flags = FRAME_SYSCALL;
	thread0.td_pcb->pcb_special.sp =
	    (u_int64_t)thread0.td_frame - 16;
	thread0.td_pcb->pcb_special.bspstore = thread0.td_kstack;

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap();

	/*
	 * Initialize debuggers, and break into them if appropriate.
	 */
	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS,
		    "Boot flags requested debugger\n");
#endif

	ia64_set_tpr(0);
	ia64_srlz_d();

	ret.bspstore = thread0.td_pcb->pcb_special.bspstore;
	ret.sp = thread0.td_pcb->pcb_special.sp;
	return (ret);
}

uint64_t
ia64_get_hcdp(void)
{

	return (bootinfo->bi_hcdp);
}

void
bzero(void *buf, size_t len)
{
	caddr_t p = buf;

	while (((vm_offset_t) p & (sizeof(u_long) - 1)) && len) {
		*p++ = 0;
		len--;
	}
	while (len >= sizeof(u_long) * 8) {
		*(u_long*) p = 0;
		*((u_long*) p + 1) = 0;
		*((u_long*) p + 2) = 0;
		*((u_long*) p + 3) = 0;
		len -= sizeof(u_long) * 8;
		*((u_long*) p + 4) = 0;
		*((u_long*) p + 5) = 0;
		*((u_long*) p + 6) = 0;
		*((u_long*) p + 7) = 0;
		p += sizeof(u_long) * 8;
	}
	while (len >= sizeof(u_long)) {
		*(u_long*) p = 0;
		len -= sizeof(u_long);
		p += sizeof(u_long);
	}
	while (len) {
		*p++ = 0;
		len--;
	}
}

u_int
ia64_itc_freq(void)
{

	return (itc_freq);
}

void
DELAY(int n)
{
	u_int64_t start, end, now;

	sched_pin();

	start = ia64_get_itc();
	end = start + itc_freq * n;
	/* printf("DELAY from 0x%lx to 0x%lx\n", start, end); */
	do {
		now = ia64_get_itc();
	} while (now < end || (now > start && end < start));

	sched_unpin();
}

/*
 * Send an interrupt (signal) to a process.
 */
void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct proc *p;
	struct thread *td;
	struct trapframe *tf;
	struct sigacts *psp;
	struct sigframe sf, *sfp;
	u_int64_t sbs, sp;
	int oonstack;
	int sig;
	u_long code;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	sp = tf->tf_special.sp;
	oonstack = sigonstack(sp);
	sbs = 0;

	/* save user context */
	bzero(&sf, sizeof(struct sigframe));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sbs = (u_int64_t)td->td_sigstk.ss_sp;
		sbs = (sbs + 15) & ~15;
		sfp = (struct sigframe *)(sbs + td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sfp = (struct sigframe *)sp;
	sfp = (struct sigframe *)((u_int64_t)(sfp - 1) & ~15);

	/* Fill in the siginfo structure for POSIX handlers. */
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		sf.sf_si = ksi->ksi_info;
		sf.sf_si.si_signo = sig;
		/*
		 * XXX this shouldn't be here after code in trap.c
		 * is fixed
		 */
		sf.sf_si.si_addr = (void*)tf->tf_special.ifa;
		code = (u_int64_t)&sfp->sf_si;
	}

	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	get_mcontext(td, &sf.sf_uc.uc_mcontext, 0);

	/* Copy the frame out to userland. */
	if (copyout(&sf, sfp, sizeof(sf)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		return;
	}

	if ((tf->tf_flags & FRAME_SYSCALL) == 0) {
		tf->tf_special.psr &= ~IA64_PSR_RI;
		tf->tf_special.iip = ia64_get_k5() +
		    ((uint64_t)break_sigtramp - (uint64_t)ia64_gateway_page);
	} else
		tf->tf_special.iip = ia64_get_k5() +
		    ((uint64_t)epc_sigtramp - (uint64_t)ia64_gateway_page);

	/*
	 * Setup the trapframe to return to the signal trampoline. We pass
	 * information to the trampoline in the following registers:
	 *
	 *	gp	new backing store or NULL
	 *	r8	signal number
	 *	r9	signal code or siginfo pointer
	 *	r10	signal handler (function descriptor)
	 */
	tf->tf_special.sp = (u_int64_t)sfp - 16;
	tf->tf_special.gp = sbs;
	tf->tf_special.bspstore = sf.sf_uc.uc_mcontext.mc_special.bspstore;
	tf->tf_special.ndirty = 0;
	tf->tf_special.rnat = sf.sf_uc.uc_mcontext.mc_special.rnat;
	tf->tf_scratch.gr8 = sig;
	tf->tf_scratch.gr9 = code;
	tf->tf_scratch.gr10 = (u_int64_t)catcher;

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 *
 * MPSAFE
 */
int
sys_sigreturn(struct thread *td,
	struct sigreturn_args /* {
		ucontext_t *sigcntxp;
	} */ *uap)
{
	ucontext_t uc;
	struct trapframe *tf;
	struct pcb *pcb;

	tf = td->td_frame;
	pcb = td->td_pcb;

	/*
	 * Fetch the entire context structure at once for speed.
	 * We don't use a normal argument to simplify RSE handling.
	 */
	if (copyin(uap->sigcntxp, (caddr_t)&uc, sizeof(uc)))
		return (EFAULT);

	set_mcontext(td, &uc.uc_mcontext);

#if defined(COMPAT_43)
	if (sigonstack(tf->tf_special.sp))
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	else
		td->td_sigstk.ss_flags &= ~SS_ONSTACK;
#endif
	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	return (EJUSTRETURN);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sigreturn(struct thread *td, struct freebsd4_sigreturn_args *uap)
{

	return sys_sigreturn(td, (struct sigreturn_args *)uap);
}
#endif

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	pcb->pcb_special = tf->tf_special;
	pcb->pcb_special.__spare = ~0UL;	/* XXX see unwind.c */
	save_callee_saved(&pcb->pcb_preserved);
	save_callee_saved_fp(&pcb->pcb_preserved_fp);
}

int
ia64_flush_dirty(struct thread *td, struct _special *r)
{
	struct iovec iov;
	struct uio uio;
	uint64_t bspst, kstk, rnat;
	int error, locked;

	if (r->ndirty == 0)
		return (0);

	kstk = td->td_kstack + (r->bspstore & 0x1ffUL);
	if (td == curthread) {
		__asm __volatile("mov	ar.rsc=0;;");
		__asm __volatile("mov	%0=ar.bspstore" : "=r"(bspst));
		/* Make sure we have all the user registers written out. */
		if (bspst - kstk < r->ndirty) {
			__asm __volatile("flushrs;;");
			__asm __volatile("mov	%0=ar.bspstore" : "=r"(bspst));
		}
		__asm __volatile("mov	%0=ar.rnat;;" : "=r"(rnat));
		__asm __volatile("mov	ar.rsc=3");
		error = copyout((void*)kstk, (void*)r->bspstore, r->ndirty);
		kstk += r->ndirty;
		r->rnat = (bspst > kstk && (bspst & 0x1ffL) < (kstk & 0x1ffL))
		    ? *(uint64_t*)(kstk | 0x1f8L) : rnat;
	} else {
		locked = PROC_LOCKED(td->td_proc);
		if (!locked)
			PHOLD(td->td_proc);
		iov.iov_base = (void*)(uintptr_t)kstk;
		iov.iov_len = r->ndirty;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = r->bspstore;
		uio.uio_resid = r->ndirty;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = UIO_WRITE;
		uio.uio_td = td;
		error = proc_rwmem(td->td_proc, &uio);
		/*
		 * XXX proc_rwmem() doesn't currently return ENOSPC,
		 * so I think it can bogusly return 0. Neither do
		 * we allow short writes.
		 */
		if (uio.uio_resid != 0 && error == 0)
			error = ENOSPC;
		if (!locked)
			PRELE(td->td_proc);
	}

	r->bspstore += r->ndirty;
	r->ndirty = 0;
	return (error);
}

int
get_mcontext(struct thread *td, mcontext_t *mc, int flags)
{
	struct trapframe *tf;
	int error;

	tf = td->td_frame;
	bzero(mc, sizeof(*mc));
	mc->mc_special = tf->tf_special;
	error = ia64_flush_dirty(td, &mc->mc_special);
	if (tf->tf_flags & FRAME_SYSCALL) {
		mc->mc_flags |= _MC_FLAGS_SYSCALL_CONTEXT;
		mc->mc_scratch = tf->tf_scratch;
		if (flags & GET_MC_CLEAR_RET) {
			mc->mc_scratch.gr8 = 0;
			mc->mc_scratch.gr9 = 0;
			mc->mc_scratch.gr10 = 0;
			mc->mc_scratch.gr11 = 0;
		}
	} else {
		mc->mc_flags |= _MC_FLAGS_ASYNC_CONTEXT;
		mc->mc_scratch = tf->tf_scratch;
		mc->mc_scratch_fp = tf->tf_scratch_fp;
		/*
		 * XXX If the thread never used the high FP registers, we
		 * probably shouldn't waste time saving them.
		 */
		ia64_highfp_save(td);
		mc->mc_flags |= _MC_FLAGS_HIGHFP_VALID;
		mc->mc_high_fp = td->td_pcb->pcb_high_fp;
	}
	save_callee_saved(&mc->mc_preserved);
	save_callee_saved_fp(&mc->mc_preserved_fp);
	return (error);
}

int
set_mcontext(struct thread *td, const mcontext_t *mc)
{
	struct _special s;
	struct trapframe *tf;
	uint64_t psrmask;

	tf = td->td_frame;

	KASSERT((tf->tf_special.ndirty & ~PAGE_MASK) == 0,
	    ("Whoa there! We have more than 8KB of dirty registers!"));

	s = mc->mc_special;
	/*
	 * Only copy the user mask and the restart instruction bit from
	 * the new context.
	 */
	psrmask = IA64_PSR_BE | IA64_PSR_UP | IA64_PSR_AC | IA64_PSR_MFL |
	    IA64_PSR_MFH | IA64_PSR_RI;
	s.psr = (tf->tf_special.psr & ~psrmask) | (s.psr & psrmask);
	/* We don't have any dirty registers of the new context. */
	s.ndirty = 0;
	if (mc->mc_flags & _MC_FLAGS_ASYNC_CONTEXT) {
		/*
		 * We can get an async context passed to us while we
		 * entered the kernel through a syscall: sigreturn(2)
		 * takes contexts that could previously be the result of
		 * a trap or interrupt.
		 * Hence, we cannot assert that the trapframe is not
		 * a syscall frame, but we can assert that it's at
		 * least an expected syscall.
		 */
		if (tf->tf_flags & FRAME_SYSCALL) {
			KASSERT(tf->tf_scratch.gr15 == SYS_sigreturn, ("foo"));
			tf->tf_flags &= ~FRAME_SYSCALL;
		}
		tf->tf_scratch = mc->mc_scratch;
		tf->tf_scratch_fp = mc->mc_scratch_fp;
		if (mc->mc_flags & _MC_FLAGS_HIGHFP_VALID)
			td->td_pcb->pcb_high_fp = mc->mc_high_fp;
	} else {
		KASSERT((tf->tf_flags & FRAME_SYSCALL) != 0, ("foo"));
		if ((mc->mc_flags & _MC_FLAGS_SYSCALL_CONTEXT) == 0) {
			s.cfm = tf->tf_special.cfm;
			s.iip = tf->tf_special.iip;
			tf->tf_scratch.gr15 = 0;	/* Clear syscall nr. */
		} else
			tf->tf_scratch = mc->mc_scratch;
	}
	tf->tf_special = s;
	restore_callee_saved(&mc->mc_preserved);
	restore_callee_saved_fp(&mc->mc_preserved_fp);

	return (0);
}

/*
 * Clear registers on exec.
 */
void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct trapframe *tf;
	uint64_t *ksttop, *kst;

	tf = td->td_frame;
	ksttop = (uint64_t*)(td->td_kstack + tf->tf_special.ndirty +
	    (tf->tf_special.bspstore & 0x1ffUL));

	/*
	 * We can ignore up to 8KB of dirty registers by masking off the
	 * lower 13 bits in exception_restore() or epc_syscall(). This
	 * should be enough for a couple of years, but if there are more
	 * than 8KB of dirty registers, we lose track of the bottom of
	 * the kernel stack. The solution is to copy the active part of
	 * the kernel stack down 1 page (or 2, but not more than that)
	 * so that we always have less than 8KB of dirty registers.
	 */
	KASSERT((tf->tf_special.ndirty & ~PAGE_MASK) == 0,
	    ("Whoa there! We have more than 8KB of dirty registers!"));

	bzero(&tf->tf_special, sizeof(tf->tf_special));
	if ((tf->tf_flags & FRAME_SYSCALL) == 0) {	/* break syscalls. */
		bzero(&tf->tf_scratch, sizeof(tf->tf_scratch));
		bzero(&tf->tf_scratch_fp, sizeof(tf->tf_scratch_fp));
		tf->tf_special.cfm = (1UL<<63) | (3UL<<7) | 3UL;
		tf->tf_special.bspstore = IA64_BACKINGSTORE;
		/*
		 * Copy the arguments onto the kernel register stack so that
		 * they get loaded by the loadrs instruction. Skip over the
		 * NaT collection points.
		 */
		kst = ksttop - 1;
		if (((uintptr_t)kst & 0x1ff) == 0x1f8)
			*kst-- = 0;
		*kst-- = 0;
		if (((uintptr_t)kst & 0x1ff) == 0x1f8)
			*kst-- = 0;
		*kst-- = imgp->ps_strings;
		if (((uintptr_t)kst & 0x1ff) == 0x1f8)
			*kst-- = 0;
		*kst = stack;
		tf->tf_special.ndirty = (ksttop - kst) << 3;
	} else {				/* epc syscalls (default). */
		tf->tf_special.cfm = (3UL<<62) | (3UL<<7) | 3UL;
		tf->tf_special.bspstore = IA64_BACKINGSTORE + 24;
		/*
		 * Write values for out0, out1 and out2 to the user's backing
		 * store and arrange for them to be restored into the user's
		 * initial register frame.
		 * Assumes that (bspstore & 0x1f8) < 0x1e0.
		 */
		suword((caddr_t)tf->tf_special.bspstore - 24, stack);
		suword((caddr_t)tf->tf_special.bspstore - 16, imgp->ps_strings);
		suword((caddr_t)tf->tf_special.bspstore -  8, 0);
	}

	tf->tf_special.iip = imgp->entry_addr;
	tf->tf_special.sp = (stack & ~15) - 16;
	tf->tf_special.rsc = 0xf;
	tf->tf_special.fpsr = IA64_FPSR_DEFAULT;
	tf->tf_special.psr = IA64_PSR_IC | IA64_PSR_I | IA64_PSR_IT |
	    IA64_PSR_DT | IA64_PSR_RT | IA64_PSR_DFH | IA64_PSR_BN |
	    IA64_PSR_CPL_USER;
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	uint64_t slot;

	switch (addr & 0xFUL) {
	case 0:
		slot = IA64_PSR_RI_0;
		break;
	case 1:
		/* XXX we need to deal with MLX bundles here */
		slot = IA64_PSR_RI_1;
		break;
	case 2:
		slot = IA64_PSR_RI_2;
		break;
	default:
		return (EINVAL);
	}

	td->td_frame->tf_special.iip = addr & ~0x0FULL;
	td->td_frame->tf_special.psr =
	    (td->td_frame->tf_special.psr & ~IA64_PSR_RI) | slot;
	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	struct trapframe *tf;

	/*
	 * There's no way to set single stepping when we're leaving the
	 * kernel through the EPC syscall path. The way we solve this is
	 * by enabling the lower-privilege trap so that we re-enter the
	 * kernel as soon as the privilege level changes. See trap.c for
	 * how we proceed from there.
	 */
	tf = td->td_frame;
	if (tf->tf_flags & FRAME_SYSCALL)
		tf->tf_special.psr |= IA64_PSR_LP;
	else
		tf->tf_special.psr |= IA64_PSR_SS;
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct trapframe *tf;

	/*
	 * Clear any and all status bits we may use to implement single
	 * stepping.
	 */
	tf = td->td_frame;
	tf->tf_special.psr &= ~IA64_PSR_SS;
	tf->tf_special.psr &= ~IA64_PSR_LP;
	tf->tf_special.psr &= ~IA64_PSR_TB;
	return (0);
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	regs->r_special = tf->tf_special;
	regs->r_scratch = tf->tf_scratch;
	save_callee_saved(&regs->r_preserved);
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;
	int error;

	tf = td->td_frame;
	error = ia64_flush_dirty(td, &tf->tf_special);
	if (!error) {
		tf->tf_special = regs->r_special;
		tf->tf_special.bspstore += tf->tf_special.ndirty;
		tf->tf_special.ndirty = 0;
		tf->tf_scratch = regs->r_scratch;
		restore_callee_saved(&regs->r_preserved);
	}
	return (error);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct trapframe *frame = td->td_frame;
	struct pcb *pcb = td->td_pcb;

	/* Save the high FP registers. */
	ia64_highfp_save(td);

	fpregs->fpr_scratch = frame->tf_scratch_fp;
	save_callee_saved_fp(&fpregs->fpr_preserved);
	fpregs->fpr_high = pcb->pcb_high_fp;
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct trapframe *frame = td->td_frame;
	struct pcb *pcb = td->td_pcb;

	/* Throw away the high FP registers (should be redundant). */
	ia64_highfp_drop(td);

	frame->tf_scratch_fp = fpregs->fpr_scratch;
	restore_callee_saved_fp(&fpregs->fpr_preserved);
	pcb->pcb_high_fp = fpregs->fpr_high;
	return (0);
}

void
ia64_sync_icache(vm_offset_t va, vm_offset_t sz)
{
	vm_offset_t lim;

	if (!ia64_sync_icache_needed)
		return;

	lim = va + sz;
	while (va < lim) {
		ia64_fc_i(va);
		va += 32;	/* XXX */
	}

	ia64_sync_i();
	ia64_srlz_i();
}
