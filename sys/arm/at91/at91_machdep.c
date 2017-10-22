/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * machdep.c
 *
 * Machine dependant functions for kernel setup
 *
 * This file needs a lot of work.
 *
 * Created      : 17/09/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/cons.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/kdb.h>
#include <sys/msgbuf.h>
#include <machine/reg.h>
#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <sys/reboot.h>

#include <arm/at91/at91board.h>
#include <arm/at91/at91var.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91sam9g20reg.h>

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define KERNEL_PT_KERN		1
#define KERNEL_PT_KERN_NUM	22
#define KERNEL_PT_AFKERNEL	KERNEL_PT_KERN + KERNEL_PT_KERN_NUM	/* L2 table for mapping after kernel */
#define	KERNEL_PT_AFKERNEL_NUM	5

/* this should be evenly divisable by PAGE_SIZE / L2_TABLE_SIZE_REAL (or 4) */
#define NUM_KERNEL_PTS		(KERNEL_PT_AFKERNEL + KERNEL_PT_AFKERNEL_NUM)

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

struct pv_addr kernel_pt_table[NUM_KERNEL_PTS];

extern void *_end;

extern int *end;

struct pcpu __pcpu;
struct pcpu *pcpup = &__pcpu;

/* Physical and virtual addresses for some global pages */

vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];
vm_offset_t physical_pages;

struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
struct pv_addr kernelstack;

static void *boot_arg1;
static void *boot_arg2;

static struct trapframe proc0_tf;

/* Static device mappings. */
const struct pmap_devmap at91_devmap[] = {
	/*
	 * Map the on-board devices VA == PA so that we can access them
	 * with the MMU on or off.
	 */
	{
		/*
		 * This at least maps the interrupt controller, the UART
		 * and the timer. Other devices should use newbus to
		 * map their memory anyway.
		 */
		0xdff00000,
		0xfff00000,
		0x00100000,
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	/* We can't just map the OHCI registers VA == PA, because
	 * AT91xx_xxx_BASE belongs to the userland address space.
	 * We could just choose a different virtual address, but a better
	 * solution would probably be to just use pmap_mapdev() to allocate
	 * KVA, as we don't need the OHCI controller before the vm
	 * initialization is done. However, the AT91 resource allocation
	 * system doesn't know how to use pmap_mapdev() yet.
	 * Care must be taken to ensure PA and VM address do not overlap
	 * between entries.
	 */
	{
		/*
		 * Add the ohci controller, and anything else that might be
		 * on this chip select for a VA/PA mapping.
		 */
		/* Internal Memory 1MB  */
		AT91RM92_OHCI_BASE,
		AT91RM92_OHCI_PA_BASE,
		0x00100000,
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{
		/* CompactFlash controller. Portion of EBI CS4 1MB */
		AT91RM92_CF_BASE,
		AT91RM92_CF_PA_BASE,
		0x00100000,
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	/* The next two should be good for the 9260, 9261 and 9G20 since
	 * addresses mapping is the same. */
	{
		/* Internal Memory 1MB  */
		AT91SAM9G20_OHCI_BASE,
		AT91SAM9G20_OHCI_PA_BASE,
		0x00100000,
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{
		/* EBI CS3 256MB */
		AT91SAM9G20_NAND_BASE,
		AT91SAM9G20_NAND_PA_BASE,
		AT91SAM9G20_NAND_SIZE,
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ 0, 0, 0, 0, 0, }
};

long
at91_ramsize(void)
{
	uint32_t *SDRAMC = (uint32_t *)(AT91_BASE + AT91RM92_SDRAMC_BASE);
	uint32_t cr, mr;
	int banks, rows, cols, bw;

	if (at91_is_rm92()) {
		SDRAMC = (uint32_t *)(AT91_BASE + AT91RM92_SDRAMC_BASE);
		cr = SDRAMC[AT91RM92_SDRAMC_CR / 4];
		mr = SDRAMC[AT91RM92_SDRAMC_MR / 4];
		banks = (cr & AT91RM92_SDRAMC_CR_NB_4) ? 2 : 1;
		rows = ((cr & AT91RM92_SDRAMC_CR_NR_MASK) >> 2) + 11;
		cols = (cr & AT91RM92_SDRAMC_CR_NC_MASK) + 8;
		bw = (mr & AT91RM92_SDRAMC_MR_DBW_16) ? 1 : 2;
	} else {
		/* This should be good for the 9260, 9261 and 9G20 as addresses
		 * and registers are the same */
		SDRAMC = (uint32_t *)(AT91_BASE + AT91SAM9G20_SDRAMC_BASE);
		cr = SDRAMC[AT91SAM9G20_SDRAMC_CR / 4];
		mr = SDRAMC[AT91SAM9G20_SDRAMC_MR / 4];
		banks = (cr & AT91SAM9G20_SDRAMC_CR_NB_4) ? 2 : 1;
		rows = ((cr & AT91SAM9G20_SDRAMC_CR_NR_MASK) >> 2) + 11;
		cols = (cr & AT91SAM9G20_SDRAMC_CR_NC_MASK) + 8;
		bw = (cr & AT91SAM9G20_SDRAMC_CR_DBW_16) ? 1 : 2;
	}

	return (1 << (cols + rows + banks + bw));
}

void *
initarm(void *arg, void *arg2)
{
	struct pv_addr  kernel_l1pt;
	struct pv_addr  dpcpu;
	int loop, i;
	u_int l1pagetable;
	vm_offset_t freemempos;
	vm_offset_t afterkern;
	uint32_t memsize;
	vm_offset_t lastaddr;

	boot_arg1 = arg;
	boot_arg2 = arg2;
	set_cpufuncs();
	lastaddr = fake_preload_metadata();
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);

	/* Do basic tuning, hz etc */
	init_param1();

	freemempos = (lastaddr + PAGE_MASK) & ~PAGE_MASK;
	/* Define a macro to simplify memory allocation */
#define valloc_pages(var, np)                   \
	alloc_pages((var).pv_va, (np));         \
	(var).pv_pa = (var).pv_va + (KERNPHYSADDR - KERNVIRTADDR);

#define alloc_pages(var, np)			\
	(var) = freemempos;		\
	freemempos += (np * PAGE_SIZE);		\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos += PAGE_SIZE;
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);
	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		if (!(loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[loop],
			    L2_TABLE_SIZE / PAGE_SIZE);
		} else {
			kernel_pt_table[loop].pv_va = freemempos -
			    (loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL)) *
			    L2_TABLE_SIZE_REAL;
			kernel_pt_table[loop].pv_pa =
			    kernel_pt_table[loop].pv_va - KERNVIRTADDR +
			    KERNPHYSADDR;
		}
		i++;
	}
	/*
	 * Allocate a page for the system page mapped to V0x00000000
	 * This page will just contain the system vectors and can be
	 * shared by all processes.
	 */
	valloc_pages(systempage, 1);

	/* Allocate dynamic per-cpu area. */
	valloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu.pv_va, 0);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, IRQ_STACK_SIZE);
	valloc_pages(abtstack, ABT_STACK_SIZE);
	valloc_pages(undstack, UND_STACK_SIZE);
	valloc_pages(kernelstack, KSTACK_PAGES);
	valloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);

	/*
	 * Now we start construction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/* Map the L2 pages tables in the L1 page table */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH,
	    &kernel_pt_table[KERNEL_PT_SYS]);
	for (i = 0; i < KERNEL_PT_KERN_NUM; i++)
		pmap_link_l2pt(l1pagetable, KERNBASE + i * L1_S_SIZE,
		    &kernel_pt_table[KERNEL_PT_KERN + i]);
	pmap_map_chunk(l1pagetable, KERNBASE, PHYSADDR,
	   (((uint32_t)lastaddr - KERNBASE) + PAGE_SIZE) & ~(PAGE_SIZE - 1),
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	afterkern = round_page((lastaddr + L1_S_SIZE) & ~(L1_S_SIZE - 1));
	for (i = 0; i < KERNEL_PT_AFKERNEL_NUM; i++) {
		pmap_link_l2pt(l1pagetable, afterkern + i * L1_S_SIZE,
		    &kernel_pt_table[KERNEL_PT_AFKERNEL + i]);
	}

	/* Map the vector page. */
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Map the DPCPU pages */
	pmap_map_chunk(l1pagetable, dpcpu.pv_va, dpcpu.pv_pa, DPCPU_SIZE,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Map the stack pages */
	pmap_map_chunk(l1pagetable, irqstack.pv_va, irqstack.pv_pa,
	    IRQ_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, abtstack.pv_va, abtstack.pv_pa,
	    ABT_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, undstack.pv_va, undstack.pv_pa,
	    UND_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, kernelstack.pv_va, kernelstack.pv_pa,
	    KSTACK_PAGES * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	pmap_map_chunk(l1pagetable, msgbufpv.pv_va, msgbufpv.pv_pa,
	    msgbufsize, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		pmap_map_chunk(l1pagetable, kernel_pt_table[loop].pv_va,
		    kernel_pt_table[loop].pv_pa, L2_TABLE_SIZE,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	}

	pmap_devmap_bootstrap(l1pagetable, at91_devmap);
	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT);
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2));

	cninit();

	/* Get chip id so device drivers know about differences */
	at91_chip_id = *(volatile uint32_t *)
		(AT91_BASE + AT91_DBGU_BASE + DBGU_C1R);

	memsize = board_init();
	physmem = memsize / PAGE_SIZE;

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	cpu_control(CPU_CONTROL_MMU_ENABLE, CPU_CONTROL_MMU_ENABLE);
	set_stackptr(PSR_IRQ32_MODE,
	    irqstack.pv_va + IRQ_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_ABT32_MODE,
	    abtstack.pv_va + ABT_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_UND32_MODE,
	    undstack.pv_va + UND_STACK_SIZE * PAGE_SIZE);

	/*
	 * We must now clean the cache again....
	 * Cleaning may be done by reading new data to displace any
	 * dirty data in the cache. This will have happened in setttb()
	 * but since we are boot strapping the addresses used for the read
	 * may have just been remapped and thus the cache could be out
	 * of sync. A re-clean after the switch will cure this.
	 * After booting there are no gross relocations of the kernel thus
	 * this problem will not occur after initarm().
	 */
	cpu_idcache_wbinv_all();

	/* Set stack for exception handlers */

	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;
	undefined_init();

	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kernelstack.pv_va;
	thread0.td_pcb = (struct pcb *)
		(thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;

	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);

	pmap_curmaxkvaddr = afterkern + L1_S_SIZE * (KERNEL_PT_KERN_NUM - 1);

	/*
	 * ARM_USE_SMALL_ALLOC uses dump_avail, so it must be filled before
	 * calling pmap_bootstrap.
	 */
	dump_avail[0] = PHYSADDR;
	dump_avail[1] = PHYSADDR + memsize;
	dump_avail[2] = 0;
	dump_avail[3] = 0;

	pmap_bootstrap(freemempos,
	    KERNVIRTADDR + 3 * memsize,
	    &kernel_l1pt);
	msgbufp = (void*)msgbufpv.pv_va;
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();

	i = 0;
#if PHYSADDR != KERNPHYSADDR
	phys_avail[i++] = PHYSADDR;
	phys_avail[i++] = KERNPHYSADDR;
#endif
	phys_avail[i++] = virtual_avail - KERNVIRTADDR + KERNPHYSADDR;
	phys_avail[i++] = PHYSADDR + memsize;
	phys_avail[i++] = 0;
	phys_avail[i++] = 0;
	init_param2(physmem);
	kdb_init();
	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}
