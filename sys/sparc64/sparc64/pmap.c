/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      from:   @(#)pmap.c      7.7 (Berkeley)  5/12/91
 * $FreeBSD: release/7.0.0/sys/sparc64/sparc64/pmap.c 175495 2008-01-19 18:15:07Z kib $
 */

/*
 * Manages physical address maps.
 *
 * In addition to hardware address maps, this module is called upon to
 * provide software-use-only maps which may or may not be stored in the
 * same form as hardware maps.  These pseudo-maps are used to store
 * intermediate results from copy operations to and from address spaces.
 *
 * Since the information managed by this module is also stored by the
 * logical address mapping module, this module may throw away valid virtual
 * to physical mappings at almost any time.  However, invalidations of
 * mappings must be done as requested.
 *
 * In order to cope with hardware architectures which make virtual to
 * physical map invalidates expensive, this module may delay invalidate
 * reduced protection operations until such time as they are actually
 * necessary.  This module is given full information as to which processors
 * are currently using which maps, and to when physical maps must be made
 * correct.
 */

#include "opt_kstack_pages.h"
#include "opt_msgbuf.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

#include <machine/cache.h>
#include <machine/frame.h>
#include <machine/instr.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_mem.h>
#include <machine/smp.h>
#include <machine/tlb.h>
#include <machine/tte.h>
#include <machine/tsb.h>

#define	PMAP_DEBUG

#ifndef	PMAP_SHPGPERPROC
#define	PMAP_SHPGPERPROC	200
#endif

/* XXX */
#include "opt_sched.h"
#ifndef SCHED_4BSD
#error "sparc64 only works with SCHED_4BSD which uses a global scheduler lock."
#endif
extern struct mtx sched_lock;

/*
 * Virtual and physical address of message buffer.
 */
struct msgbuf *msgbufp;
vm_paddr_t msgbuf_phys;

/*
 * Map of physical memory reagions.
 */
vm_paddr_t phys_avail[128];
static struct ofw_mem_region mra[128];
struct ofw_mem_region sparc64_memreg[128];
int sparc64_nmemreg;
static struct ofw_map translations[128];
static int translations_size;

static vm_offset_t pmap_idle_map;
static vm_offset_t pmap_temp_map_1;
static vm_offset_t pmap_temp_map_2;

/*
 * First and last available kernel virtual addresses.
 */
vm_offset_t virtual_avail;
vm_offset_t virtual_end;
vm_offset_t kernel_vm_end;

vm_offset_t vm_max_kernel_address;

/*
 * Kernel pmap.
 */
struct pmap kernel_pmap_store;

/*
 * Allocate physical memory for use in pmap_bootstrap.
 */
static vm_paddr_t pmap_bootstrap_alloc(vm_size_t size);

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 *
 * The page queues and pmap must be locked.
 */
static void pmap_enter_locked(pmap_t pm, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, boolean_t wired);

extern int tl1_immu_miss_patch_1[];
extern int tl1_immu_miss_patch_2[];
extern int tl1_dmmu_miss_patch_1[];
extern int tl1_dmmu_miss_patch_2[];
extern int tl1_dmmu_prot_patch_1[];
extern int tl1_dmmu_prot_patch_2[];

/*
 * If user pmap is processed with pmap_remove and with pmap_remove and the
 * resident count drops to 0, there are no more pages to remove, so we
 * need not continue.
 */
#define	PMAP_REMOVE_DONE(pm) \
	((pm) != kernel_pmap && (pm)->pm_stats.resident_count == 0)

/*
 * The threshold (in bytes) above which tsb_foreach() is used in pmap_remove()
 * and pmap_protect() instead of trying each virtual address.
 */
#define	PMAP_TSB_THRESH	((TSB_SIZE / 2) * PAGE_SIZE)

SYSCTL_NODE(_debug, OID_AUTO, pmap_stats, CTLFLAG_RD, 0, "");

PMAP_STATS_VAR(pmap_nenter);
PMAP_STATS_VAR(pmap_nenter_update);
PMAP_STATS_VAR(pmap_nenter_replace);
PMAP_STATS_VAR(pmap_nenter_new);
PMAP_STATS_VAR(pmap_nkenter);
PMAP_STATS_VAR(pmap_nkenter_oc);
PMAP_STATS_VAR(pmap_nkenter_stupid);
PMAP_STATS_VAR(pmap_nkremove);
PMAP_STATS_VAR(pmap_nqenter);
PMAP_STATS_VAR(pmap_nqremove);
PMAP_STATS_VAR(pmap_ncache_enter);
PMAP_STATS_VAR(pmap_ncache_enter_c);
PMAP_STATS_VAR(pmap_ncache_enter_oc);
PMAP_STATS_VAR(pmap_ncache_enter_cc);
PMAP_STATS_VAR(pmap_ncache_enter_coc);
PMAP_STATS_VAR(pmap_ncache_enter_nc);
PMAP_STATS_VAR(pmap_ncache_enter_cnc);
PMAP_STATS_VAR(pmap_ncache_remove);
PMAP_STATS_VAR(pmap_ncache_remove_c);
PMAP_STATS_VAR(pmap_ncache_remove_oc);
PMAP_STATS_VAR(pmap_ncache_remove_cc);
PMAP_STATS_VAR(pmap_ncache_remove_coc);
PMAP_STATS_VAR(pmap_ncache_remove_nc);
PMAP_STATS_VAR(pmap_nzero_page);
PMAP_STATS_VAR(pmap_nzero_page_c);
PMAP_STATS_VAR(pmap_nzero_page_oc);
PMAP_STATS_VAR(pmap_nzero_page_nc);
PMAP_STATS_VAR(pmap_nzero_page_area);
PMAP_STATS_VAR(pmap_nzero_page_area_c);
PMAP_STATS_VAR(pmap_nzero_page_area_oc);
PMAP_STATS_VAR(pmap_nzero_page_area_nc);
PMAP_STATS_VAR(pmap_nzero_page_idle);
PMAP_STATS_VAR(pmap_nzero_page_idle_c);
PMAP_STATS_VAR(pmap_nzero_page_idle_oc);
PMAP_STATS_VAR(pmap_nzero_page_idle_nc);
PMAP_STATS_VAR(pmap_ncopy_page);
PMAP_STATS_VAR(pmap_ncopy_page_c);
PMAP_STATS_VAR(pmap_ncopy_page_oc);
PMAP_STATS_VAR(pmap_ncopy_page_nc);
PMAP_STATS_VAR(pmap_ncopy_page_dc);
PMAP_STATS_VAR(pmap_ncopy_page_doc);
PMAP_STATS_VAR(pmap_ncopy_page_sc);
PMAP_STATS_VAR(pmap_ncopy_page_soc);

PMAP_STATS_VAR(pmap_nnew_thread);
PMAP_STATS_VAR(pmap_nnew_thread_oc);

/*
 * Quick sort callout for comparing memory regions.
 */
static int mr_cmp(const void *a, const void *b);
static int om_cmp(const void *a, const void *b);
static int
mr_cmp(const void *a, const void *b)
{
	const struct ofw_mem_region *mra;
	const struct ofw_mem_region *mrb;

	mra = a;
	mrb = b;
	if (mra->mr_start < mrb->mr_start)
		return (-1);
	else if (mra->mr_start > mrb->mr_start)
		return (1);
	else
		return (0);
}
static int
om_cmp(const void *a, const void *b)
{
	const struct ofw_map *oma;
	const struct ofw_map *omb;

	oma = a;
	omb = b;
	if (oma->om_start < omb->om_start)
		return (-1);
	else if (oma->om_start > omb->om_start)
		return (1);
	else
		return (0);
}

/*
 * Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t ekva)
{
	struct pmap *pm;
	struct tte *tp;
	vm_offset_t off;
	vm_offset_t va;
	vm_paddr_t pa;
	vm_size_t physsz;
	vm_size_t virtsz;
	ihandle_t pmem;
	ihandle_t vmem;
	int sz;
	int i;
	int j;

	/*
	 * Find out what physical memory is available from the prom and
	 * initialize the phys_avail array.  This must be done before
	 * pmap_bootstrap_alloc is called.
	 */
	if ((pmem = OF_finddevice("/memory")) == -1)
		panic("pmap_bootstrap: finddevice /memory");
	if ((sz = OF_getproplen(pmem, "available")) == -1)
		panic("pmap_bootstrap: getproplen /memory/available");
	if (sizeof(phys_avail) < sz)
		panic("pmap_bootstrap: phys_avail too small");
	if (sizeof(mra) < sz)
		panic("pmap_bootstrap: mra too small");
	bzero(mra, sz);
	if (OF_getprop(pmem, "available", mra, sz) == -1)
		panic("pmap_bootstrap: getprop /memory/available");
	sz /= sizeof(*mra);
	CTR0(KTR_PMAP, "pmap_bootstrap: physical memory");
	qsort(mra, sz, sizeof (*mra), mr_cmp);
	physsz = 0;
	getenv_quad("hw.physmem", &physmem);
	physmem = btoc(physmem);
	for (i = 0, j = 0; i < sz; i++, j += 2) {
		CTR2(KTR_PMAP, "start=%#lx size=%#lx", mra[i].mr_start,
		    mra[i].mr_size);
		if (physmem != 0 && btoc(physsz + mra[i].mr_size) >= physmem) {
			if (btoc(physsz) < physmem) {
				phys_avail[j] = mra[i].mr_start;
				phys_avail[j + 1] = mra[i].mr_start +
				    (ctob(physmem) - physsz);
				physsz = ctob(physmem);
			}
			break;
		}
		phys_avail[j] = mra[i].mr_start;
		phys_avail[j + 1] = mra[i].mr_start + mra[i].mr_size;
		physsz += mra[i].mr_size;
	}
	physmem = btoc(physsz);

	/*
	 * Calculate the size of kernel virtual memory, and the size and mask
	 * for the kernel tsb.
	 */
	virtsz = roundup(physsz, PAGE_SIZE_4M << (PAGE_SHIFT - TTE_SHIFT));
	vm_max_kernel_address = VM_MIN_KERNEL_ADDRESS + virtsz;
	tsb_kernel_size = virtsz >> (PAGE_SHIFT - TTE_SHIFT);
	tsb_kernel_mask = (tsb_kernel_size >> TTE_SHIFT) - 1;

	/*
	 * Allocate the kernel tsb and lock it in the tlb.
	 */
	pa = pmap_bootstrap_alloc(tsb_kernel_size);
	if (pa & PAGE_MASK_4M)
		panic("pmap_bootstrap: tsb unaligned\n");
	tsb_kernel_phys = pa;
	tsb_kernel = (struct tte *)(VM_MIN_KERNEL_ADDRESS - tsb_kernel_size);
	pmap_map_tsb();
	bzero(tsb_kernel, tsb_kernel_size);

	/*
	 * Allocate and map the message buffer.
	 */
	msgbuf_phys = pmap_bootstrap_alloc(MSGBUF_SIZE);
	msgbufp = (struct msgbuf *)TLB_PHYS_TO_DIRECT(msgbuf_phys);

	/*
	 * Patch the virtual address and the tsb mask into the trap table.
	 */

#define	SETHI(rd, imm22) \
	(EIF_OP(IOP_FORM2) | EIF_F2_RD(rd) | EIF_F2_OP2(INS0_SETHI) | \
	    EIF_IMM((imm22) >> 10, 22))
#define	OR_R_I_R(rd, imm13, rs1) \
	(EIF_OP(IOP_MISC) | EIF_F3_RD(rd) | EIF_F3_OP3(INS2_OR) | \
	    EIF_F3_RS1(rs1) | EIF_F3_I(1) | EIF_IMM(imm13, 13))

#define	PATCH(addr) do { \
	if (addr[0] != SETHI(IF_F2_RD(addr[0]), 0x0) || \
	    addr[1] != OR_R_I_R(IF_F3_RD(addr[1]), 0x0, IF_F3_RS1(addr[1])) || \
	    addr[2] != SETHI(IF_F2_RD(addr[2]), 0x0)) \
		panic("pmap_boostrap: patched instructions have changed"); \
	addr[0] |= EIF_IMM((tsb_kernel_mask) >> 10, 22); \
	addr[1] |= EIF_IMM(tsb_kernel_mask, 10); \
	addr[2] |= EIF_IMM(((vm_offset_t)tsb_kernel) >> 10, 22); \
	flush(addr); \
	flush(addr + 1); \
	flush(addr + 2); \
} while (0)

	PATCH(tl1_immu_miss_patch_1);
	PATCH(tl1_immu_miss_patch_2);
	PATCH(tl1_dmmu_miss_patch_1);
	PATCH(tl1_dmmu_miss_patch_2);
	PATCH(tl1_dmmu_prot_patch_1);
	PATCH(tl1_dmmu_prot_patch_2);
	
	/*
	 * Enter fake 8k pages for the 4MB kernel pages, so that
	 * pmap_kextract() will work for them.
	 */
	for (i = 0; i < kernel_tlb_slots; i++) {
		pa = kernel_tlbs[i].te_pa;
		va = kernel_tlbs[i].te_va;
		for (off = 0; off < PAGE_SIZE_4M; off += PAGE_SIZE) {
			tp = tsb_kvtotte(va + off);
			tp->tte_vpn = TV_VPN(va + off, TS_8K);
			tp->tte_data = TD_V | TD_8K | TD_PA(pa + off) |
			    TD_REF | TD_SW | TD_CP | TD_CV | TD_P | TD_W;
		}
	}

	/*
	 * Set the start and end of kva.  The kernel is loaded at the first
	 * available 4 meg super page, so round up to the end of the page.
	 */
	virtual_avail = roundup2(ekva, PAGE_SIZE_4M);
	virtual_end = vm_max_kernel_address;
	kernel_vm_end = vm_max_kernel_address;

	/*
	 * Allocate kva space for temporary mappings.
	 */
	pmap_idle_map = virtual_avail;
	virtual_avail += PAGE_SIZE * DCACHE_COLORS;
	pmap_temp_map_1 = virtual_avail;
	virtual_avail += PAGE_SIZE * DCACHE_COLORS;
	pmap_temp_map_2 = virtual_avail;
	virtual_avail += PAGE_SIZE * DCACHE_COLORS;

	/*
	 * Allocate a kernel stack with guard page for thread0 and map it into
	 * the kernel tsb.  We must ensure that the virtual address is coloured
	 * properly, since we're allocating from phys_avail so the memory won't
	 * have an associated vm_page_t.
	 */
	pa = pmap_bootstrap_alloc(roundup(KSTACK_PAGES, DCACHE_COLORS) *
	    PAGE_SIZE);
	kstack0_phys = pa;
	virtual_avail += roundup(KSTACK_GUARD_PAGES, DCACHE_COLORS) *
	    PAGE_SIZE;
	kstack0 = virtual_avail;
	virtual_avail += roundup(KSTACK_PAGES, DCACHE_COLORS) * PAGE_SIZE;
	KASSERT(DCACHE_COLOR(kstack0) == DCACHE_COLOR(kstack0_phys),
	    ("pmap_bootstrap: kstack0 miscoloured"));
	for (i = 0; i < KSTACK_PAGES; i++) {
		pa = kstack0_phys + i * PAGE_SIZE;
		va = kstack0 + i * PAGE_SIZE;
		tp = tsb_kvtotte(va);
		tp->tte_vpn = TV_VPN(va, TS_8K);
		tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_REF | TD_SW |
		    TD_CP | TD_CV | TD_P | TD_W;
	}

	/*
	 * Calculate the last available physical address.
	 */
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		;
	Maxmem = sparc64_btop(phys_avail[i + 1]);

	/*
	 * Add the prom mappings to the kernel tsb.
	 */
	if ((vmem = OF_finddevice("/virtual-memory")) == -1)
		panic("pmap_bootstrap: finddevice /virtual-memory");
	if ((sz = OF_getproplen(vmem, "translations")) == -1)
		panic("pmap_bootstrap: getproplen translations");
	if (sizeof(translations) < sz)
		panic("pmap_bootstrap: translations too small");
	bzero(translations, sz);
	if (OF_getprop(vmem, "translations", translations, sz) == -1)
		panic("pmap_bootstrap: getprop /virtual-memory/translations");
	sz /= sizeof(*translations);
	translations_size = sz;
	CTR0(KTR_PMAP, "pmap_bootstrap: translations");
	qsort(translations, sz, sizeof (*translations), om_cmp);
	for (i = 0; i < sz; i++) {
		CTR3(KTR_PMAP,
		    "translation: start=%#lx size=%#lx tte=%#lx",
		    translations[i].om_start, translations[i].om_size,
		    translations[i].om_tte);
		if (translations[i].om_start < VM_MIN_PROM_ADDRESS ||
		    translations[i].om_start > VM_MAX_PROM_ADDRESS)
			continue;
		for (off = 0; off < translations[i].om_size;
		    off += PAGE_SIZE) {
			va = translations[i].om_start + off;
			tp = tsb_kvtotte(va);
			tp->tte_vpn = TV_VPN(va, TS_8K);
			tp->tte_data =
			    ((translations[i].om_tte &
			      ~(TD_SOFT_MASK << TD_SOFT_SHIFT)) | TD_EXEC) +
			    off;
		}
	}

	/*
	 * Get the available physical memory ranges from /memory/reg. These
	 * are only used for kernel dumps, but it may not be wise to do prom
	 * calls in that situation.
	 */
	if ((sz = OF_getproplen(pmem, "reg")) == -1)
		panic("pmap_bootstrap: getproplen /memory/reg");
	if (sizeof(sparc64_memreg) < sz)
		panic("pmap_bootstrap: sparc64_memreg too small");
	if (OF_getprop(pmem, "reg", sparc64_memreg, sz) == -1)
		panic("pmap_bootstrap: getprop /memory/reg");
	sparc64_nmemreg = sz / sizeof(*sparc64_memreg);

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 * NOTE: PMAP_LOCK_INIT() is needed as part of the initialization
	 * but sparc64 start up is not ready to initialize mutexes yet.
	 * It is called in machdep.c.
	 */
	pm = kernel_pmap;
	for (i = 0; i < MAXCPU; i++)
		pm->pm_context[i] = TLB_CTX_KERNEL;
	pm->pm_active = ~0;

	/* XXX flush all non-locked tlb entries */
}

void
pmap_map_tsb(void)
{
	vm_offset_t va;
	vm_paddr_t pa;
	u_long data;
	u_long s;
	int i;

	s = intr_disable();

	/*
	 * Map the 4mb tsb pages.
	 */
	for (i = 0; i < tsb_kernel_size; i += PAGE_SIZE_4M) {
		va = (vm_offset_t)tsb_kernel + i;
		pa = tsb_kernel_phys + i;
		data = TD_V | TD_4M | TD_PA(pa) | TD_L | TD_CP | TD_CV |
		    TD_P | TD_W;
		/* XXX - cheetah */
		stxa(AA_DMMU_TAR, ASI_DMMU, TLB_TAR_VA(va) |
		    TLB_TAR_CTX(TLB_CTX_KERNEL));
		stxa_sync(0, ASI_DTLB_DATA_IN_REG, data);
	}

	/*
	 * Set the secondary context to be the kernel context (needed for
	 * fp block operations in the kernel and the cache code).
	 */
	stxa(AA_DMMU_SCXR, ASI_DMMU, TLB_CTX_KERNEL);
	membar(Sync);

	intr_restore(s);
}

/*
 * Allocate a physical page of memory directly from the phys_avail map.
 * Can only be called from pmap_bootstrap before avail start and end are
 * calculated.
 */
static vm_paddr_t
pmap_bootstrap_alloc(vm_size_t size)
{
	vm_paddr_t pa;
	int i;

	size = round_page(size);
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i + 1] - phys_avail[i] < size)
			continue;
		pa = phys_avail[i];
		phys_avail[i] += size;
		return (pa);
	}
	panic("pmap_bootstrap_alloc");
}

/*
 * Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.tte_list);
	m->md.color = DCACHE_COLOR(VM_PAGE_TO_PHYS(m));
	m->md.flags = 0;
	m->md.pmap = NULL;
}

/*
 * Initialize the pmap module.
 */
void
pmap_init(void)
{
	vm_offset_t addr;
	vm_size_t size;
	int result;
	int i;

	for (i = 0; i < translations_size; i++) {
		addr = translations[i].om_start;
		size = translations[i].om_size;
		if (addr < VM_MIN_PROM_ADDRESS || addr > VM_MAX_PROM_ADDRESS)
			continue;
		result = vm_map_find(kernel_map, NULL, 0, &addr, size, FALSE,
		    VM_PROT_ALL, VM_PROT_ALL, 0);
		if (result != KERN_SUCCESS || addr != translations[i].om_start)
			panic("pmap_init: vm_map_find");
	}
}

/*
 * Extract the physical page address associated with the given
 * map/virtual_address pair.
 */
vm_paddr_t
pmap_extract(pmap_t pm, vm_offset_t va)
{
	struct tte *tp;
	vm_paddr_t pa;

	if (pm == kernel_pmap)
		return (pmap_kextract(va));
	PMAP_LOCK(pm);
	tp = tsb_tte_lookup(pm, va);
	if (tp == NULL)
		pa = 0;
	else
		pa = TTE_GET_PA(tp) | (va & TTE_GET_PAGE_MASK(tp));
	PMAP_UNLOCK(pm);
	return (pa);
}

/*
 * Atomically extract and hold the physical page with the given
 * pmap and virtual address pair if that mapping permits the given
 * protection.
 */
vm_page_t
pmap_extract_and_hold(pmap_t pm, vm_offset_t va, vm_prot_t prot)
{
	struct tte *tp;
	vm_page_t m;

	m = NULL;
	vm_page_lock_queues();
	if (pm == kernel_pmap) {
		if (va >= VM_MIN_DIRECT_ADDRESS) {
			tp = NULL;
			m = PHYS_TO_VM_PAGE(TLB_DIRECT_TO_PHYS(va));
			vm_page_hold(m);
		} else {
			tp = tsb_kvtotte(va);
			if ((tp->tte_data & TD_V) == 0)
				tp = NULL;
		}
	} else {
		PMAP_LOCK(pm);
		tp = tsb_tte_lookup(pm, va);
	}
	if (tp != NULL && ((tp->tte_data & TD_SW) ||
	    (prot & VM_PROT_WRITE) == 0)) {
		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
		vm_page_hold(m);
	}
	vm_page_unlock_queues();
	if (pm != kernel_pmap)
		PMAP_UNLOCK(pm);
	return (m);
}

/*
 * Extract the physical page address associated with the given kernel virtual
 * address.
 */
vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	struct tte *tp;

	if (va >= VM_MIN_DIRECT_ADDRESS)
		return (TLB_DIRECT_TO_PHYS(va));
	tp = tsb_kvtotte(va);
	if ((tp->tte_data & TD_V) == 0)
		return (0);
	return (TTE_GET_PA(tp) | (va & TTE_GET_PAGE_MASK(tp)));
}

int
pmap_cache_enter(vm_page_t m, vm_offset_t va)
{
	struct tte *tp;
	int color;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_cache_enter: fake page"));
	PMAP_STATS_INC(pmap_ncache_enter);

	/*
	 * Find the color for this virtual address and note the added mapping.
	 */
	color = DCACHE_COLOR(va);
	m->md.colors[color]++;

	/*
	 * If all existing mappings have the same color, the mapping is
	 * cacheable.
	 */
	if (m->md.color == color) {
		KASSERT(m->md.colors[DCACHE_OTHER_COLOR(color)] == 0,
		    ("pmap_cache_enter: cacheable, mappings of other color"));
		if (m->md.color == DCACHE_COLOR(VM_PAGE_TO_PHYS(m)))
			PMAP_STATS_INC(pmap_ncache_enter_c);
		else
			PMAP_STATS_INC(pmap_ncache_enter_oc);
		return (1);
	}

	/*
	 * If there are no mappings of the other color, and the page still has
	 * the wrong color, this must be a new mapping.  Change the color to
	 * match the new mapping, which is cacheable.  We must flush the page
	 * from the cache now.
	 */
	if (m->md.colors[DCACHE_OTHER_COLOR(color)] == 0) {
		KASSERT(m->md.colors[color] == 1,
		    ("pmap_cache_enter: changing color, not new mapping"));
		dcache_page_inval(VM_PAGE_TO_PHYS(m));
		m->md.color = color;
		if (m->md.color == DCACHE_COLOR(VM_PAGE_TO_PHYS(m)))
			PMAP_STATS_INC(pmap_ncache_enter_cc);
		else
			PMAP_STATS_INC(pmap_ncache_enter_coc);
		return (1);
	}

	/*
	 * If the mapping is already non-cacheable, just return.
	 */	
	if (m->md.color == -1) {
		PMAP_STATS_INC(pmap_ncache_enter_nc);
		return (0);
	}

	PMAP_STATS_INC(pmap_ncache_enter_cnc);

	/*
	 * Mark all mappings as uncacheable, flush any lines with the other
	 * color out of the dcache, and set the color to none (-1).
	 */
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		atomic_clear_long(&tp->tte_data, TD_CV);
		tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
	}
	dcache_page_inval(VM_PAGE_TO_PHYS(m));
	m->md.color = -1;
	return (0);
}

void
pmap_cache_remove(vm_page_t m, vm_offset_t va)
{
	struct tte *tp;
	int color;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	CTR3(KTR_PMAP, "pmap_cache_remove: m=%p va=%#lx c=%d", m, va,
	    m->md.colors[DCACHE_COLOR(va)]);
	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_cache_remove: fake page"));
	KASSERT(m->md.colors[DCACHE_COLOR(va)] > 0,
	    ("pmap_cache_remove: no mappings %d <= 0",
	    m->md.colors[DCACHE_COLOR(va)]));
	PMAP_STATS_INC(pmap_ncache_remove);

	/*
	 * Find the color for this virtual address and note the removal of
	 * the mapping.
	 */
	color = DCACHE_COLOR(va);
	m->md.colors[color]--;

	/*
	 * If the page is cacheable, just return and keep the same color, even
	 * if there are no longer any mappings.
	 */
	if (m->md.color != -1) {
		if (m->md.color == DCACHE_COLOR(VM_PAGE_TO_PHYS(m)))
			PMAP_STATS_INC(pmap_ncache_remove_c);
		else
			PMAP_STATS_INC(pmap_ncache_remove_oc);
		return;
	}

	KASSERT(m->md.colors[DCACHE_OTHER_COLOR(color)] != 0,
	    ("pmap_cache_remove: uncacheable, no mappings of other color"));

	/*
	 * If the page is not cacheable (color is -1), and the number of
	 * mappings for this color is not zero, just return.  There are
	 * mappings of the other color still, so remain non-cacheable.
	 */
	if (m->md.colors[color] != 0) {
		PMAP_STATS_INC(pmap_ncache_remove_nc);
		return;
	}

	/*
	 * The number of mappings for this color is now zero.  Recache the
	 * other colored mappings, and change the page color to the other
	 * color.  There should be no lines in the data cache for this page,
	 * so flushing should not be needed.
	 */
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		atomic_set_long(&tp->tte_data, TD_CV);
		tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
	}
	m->md.color = DCACHE_OTHER_COLOR(color);

	if (m->md.color == DCACHE_COLOR(VM_PAGE_TO_PHYS(m)))
		PMAP_STATS_INC(pmap_ncache_remove_cc);
	else
		PMAP_STATS_INC(pmap_ncache_remove_coc);
}

/*
 * Map a wired page into kernel virtual address space.
 */
void
pmap_kenter(vm_offset_t va, vm_page_t m)
{
	vm_offset_t ova;
	struct tte *tp;
	vm_page_t om;
	u_long data;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_STATS_INC(pmap_nkenter);
	tp = tsb_kvtotte(va);
	CTR4(KTR_PMAP, "pmap_kenter: va=%#lx pa=%#lx tp=%p data=%#lx",
	    va, VM_PAGE_TO_PHYS(m), tp, tp->tte_data);
	if (DCACHE_COLOR(VM_PAGE_TO_PHYS(m)) != DCACHE_COLOR(va)) {
		CTR6(KTR_CT2,
	"pmap_kenter: off colour va=%#lx pa=%#lx o=%p oc=%#lx ot=%d pi=%#lx",
		    va, VM_PAGE_TO_PHYS(m), m->object,
		    m->object ? m->object->pg_color : -1,
		    m->object ? m->object->type : -1,
		    m->pindex);
		PMAP_STATS_INC(pmap_nkenter_oc);
	}
	if ((tp->tte_data & TD_V) != 0) {
		om = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
		ova = TTE_GET_VA(tp);
		if (m == om && va == ova) {
			PMAP_STATS_INC(pmap_nkenter_stupid);
			return;
		}
		TAILQ_REMOVE(&om->md.tte_list, tp, tte_link);
		pmap_cache_remove(om, ova);
		if (va != ova)
			tlb_page_demap(kernel_pmap, ova);
	}
	data = TD_V | TD_8K | VM_PAGE_TO_PHYS(m) | TD_REF | TD_SW | TD_CP |
	    TD_P | TD_W;
	if (pmap_cache_enter(m, va) != 0)
		data |= TD_CV;
	tp->tte_vpn = TV_VPN(va, TS_8K);
	tp->tte_data = data;
	TAILQ_INSERT_TAIL(&m->md.tte_list, tp, tte_link);
}

/*
 * Map a wired page into kernel virtual address space. This additionally
 * takes a flag argument wich is or'ed to the TTE data. This is used by
 * bus_space_map().
 * NOTE: if the mapping is non-cacheable, it's the caller's responsibility
 * to flush entries that might still be in the cache, if applicable.
 */
void
pmap_kenter_flags(vm_offset_t va, vm_paddr_t pa, u_long flags)
{
	struct tte *tp;

	tp = tsb_kvtotte(va);
	CTR4(KTR_PMAP, "pmap_kenter_flags: va=%#lx pa=%#lx tp=%p data=%#lx",
	    va, pa, tp, tp->tte_data);
	tp->tte_vpn = TV_VPN(va, TS_8K);
	tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_REF | TD_P | flags;
}

/*
 * Remove a wired page from kernel virtual address space.
 */
void
pmap_kremove(vm_offset_t va)
{
	struct tte *tp;
	vm_page_t m;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_STATS_INC(pmap_nkremove);
	tp = tsb_kvtotte(va);
	CTR3(KTR_PMAP, "pmap_kremove: va=%#lx tp=%p data=%#lx", va, tp,
	    tp->tte_data);
	if ((tp->tte_data & TD_V) == 0)
		return;
	m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
	TAILQ_REMOVE(&m->md.tte_list, tp, tte_link);
	pmap_cache_remove(m, va);
	TTE_ZERO(tp);
}

/*
 * Inverse of pmap_kenter_flags, used by bus_space_unmap().
 */
void
pmap_kremove_flags(vm_offset_t va)
{
	struct tte *tp;

	tp = tsb_kvtotte(va);
	CTR3(KTR_PMAP, "pmap_kremove: va=%#lx tp=%p data=%#lx", va, tp,
	    tp->tte_data);
	TTE_ZERO(tp);
}

/*
 * Map a range of physical addresses into kernel virtual address space.
 *
 * The value passed in *virt is a suggested virtual address for the mapping.
 * Architectures which can support a direct-mapped physical to virtual region
 * can return the appropriate address within that region, leaving '*virt'
 * unchanged.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{

	return (TLB_PHYS_TO_DIRECT(start));
}

/*
 * Map a list of wired pages into kernel virtual address space.  This is
 * intended for temporary mappings which do not need page modification or
 * references recorded.  Existing mappings in the region are overwritten.
 */
void
pmap_qenter(vm_offset_t sva, vm_page_t *m, int count)
{
	vm_offset_t va;
	int locked;

	PMAP_STATS_INC(pmap_nqenter);
	va = sva;
	if (!(locked = mtx_owned(&vm_page_queue_mtx)))
		vm_page_lock_queues();
	while (count-- > 0) {
		pmap_kenter(va, *m);
		va += PAGE_SIZE;
		m++;
	}
	if (!locked)
		vm_page_unlock_queues();
	tlb_range_demap(kernel_pmap, sva, va);
}

/*
 * Remove page mappings from kernel virtual address space.  Intended for
 * temporary mappings entered by pmap_qenter.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;
	int locked;

	PMAP_STATS_INC(pmap_nqremove);
	va = sva;
	if (!(locked = mtx_owned(&vm_page_queue_mtx)))
		vm_page_lock_queues();
	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	if (!locked)
		vm_page_unlock_queues();
	tlb_range_demap(kernel_pmap, sva, va);
}

/*
 * Initialize the pmap associated with process 0.
 */
void
pmap_pinit0(pmap_t pm)
{
	int i;

	PMAP_LOCK_INIT(pm);
	for (i = 0; i < MAXCPU; i++)
		pm->pm_context[i] = 0;
	pm->pm_active = 0;
	pm->pm_tsb = NULL;
	pm->pm_tsb_obj = NULL;
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

/*
 * Initialize a preallocated and zeroed pmap structure, such as one in a
 * vmspace structure.
 */
int
pmap_pinit(pmap_t pm)
{
	vm_page_t ma[TSB_PAGES];
	vm_page_t m;
	int i;

	PMAP_LOCK_INIT(pm);

	/*
	 * Allocate kva space for the tsb.
	 */
	if (pm->pm_tsb == NULL) {
		pm->pm_tsb = (struct tte *)kmem_alloc_nofault(kernel_map,
		    TSB_BSIZE);
		if (pm->pm_tsb == NULL) {
			PMAP_LOCK_DESTROY(pm);
			return (0);
		}
	}

	/*
	 * Allocate an object for it.
	 */
	if (pm->pm_tsb_obj == NULL)
		pm->pm_tsb_obj = vm_object_allocate(OBJT_DEFAULT, TSB_PAGES);

	VM_OBJECT_LOCK(pm->pm_tsb_obj);
	for (i = 0; i < TSB_PAGES; i++) {
		m = vm_page_grab(pm->pm_tsb_obj, i, VM_ALLOC_NOBUSY |
		    VM_ALLOC_RETRY | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
		m->valid = VM_PAGE_BITS_ALL;
		m->md.pmap = pm;
		ma[i] = m;
	}
	VM_OBJECT_UNLOCK(pm->pm_tsb_obj);
	pmap_qenter((vm_offset_t)pm->pm_tsb, ma, TSB_PAGES);

	for (i = 0; i < MAXCPU; i++)
		pm->pm_context[i] = -1;
	pm->pm_active = 0;
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
	return (1);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pm)
{
	vm_object_t obj;
	vm_page_t m;
	struct pcpu *pc;

	CTR2(KTR_PMAP, "pmap_release: ctx=%#x tsb=%p",
	    pm->pm_context[PCPU_GET(cpuid)], pm->pm_tsb);
	KASSERT(pmap_resident_count(pm) == 0,
	    ("pmap_release: resident pages %ld != 0",
	    pmap_resident_count(pm)));

	/*
	 * After the pmap was freed, it might be reallocated to a new process.
	 * When switching, this might lead us to wrongly assume that we need
	 * not switch contexts because old and new pmap pointer are equal.
	 * Therefore, make sure that this pmap is not referenced by any PCPU
	 * pointer any more. This could happen in two cases:
	 * - A process that referenced the pmap is currently exiting on a CPU.
	 *   However, it is guaranteed to not switch in any more after setting
	 *   its state to PRS_ZOMBIE.
	 * - A process that referenced this pmap ran on a CPU, but we switched
	 *   to a kernel thread, leaving the pmap pointer unchanged.
	 */
	mtx_lock_spin(&sched_lock);
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc->pc_pmap == pm)
			pc->pc_pmap = NULL;
	}
	mtx_unlock_spin(&sched_lock);

	obj = pm->pm_tsb_obj;
	VM_OBJECT_LOCK(obj);
	KASSERT(obj->ref_count == 1, ("pmap_release: tsbobj ref count != 1"));
	while (!TAILQ_EMPTY(&obj->memq)) {
		m = TAILQ_FIRST(&obj->memq);
		vm_page_lock_queues();
		if (vm_page_sleep_if_busy(m, FALSE, "pmaprl"))
			continue;
		KASSERT(m->hold_count == 0,
		    ("pmap_release: freeing held tsb page"));
		m->md.pmap = NULL;
		m->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
		vm_page_free_zero(m);
		vm_page_unlock_queues();
	}
	VM_OBJECT_UNLOCK(obj);
	pmap_qremove((vm_offset_t)pm->pm_tsb, TSB_PAGES);
	PMAP_LOCK_DESTROY(pm);
}

/*
 * Grow the number of kernel page table entries.  Unneeded.
 */
void
pmap_growkernel(vm_offset_t addr)
{

	panic("pmap_growkernel: can't grow kernel");
}

int
pmap_remove_tte(struct pmap *pm, struct pmap *pm2, struct tte *tp,
		vm_offset_t va)
{
	vm_page_t m;
	u_long data;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	data = atomic_readandclear_long(&tp->tte_data);
	if ((data & TD_FAKE) == 0) {
		m = PHYS_TO_VM_PAGE(TD_PA(data));
		TAILQ_REMOVE(&m->md.tte_list, tp, tte_link);
		if ((data & TD_WIRED) != 0)
			pm->pm_stats.wired_count--;
		if ((data & TD_PV) != 0) {
			if ((data & TD_W) != 0)
				vm_page_dirty(m);
			if ((data & TD_REF) != 0)
				vm_page_flag_set(m, PG_REFERENCED);
			if (TAILQ_EMPTY(&m->md.tte_list))
				vm_page_flag_clear(m, PG_WRITEABLE);
			pm->pm_stats.resident_count--;
		}
		pmap_cache_remove(m, va);
	}
	TTE_ZERO(tp);
	if (PMAP_REMOVE_DONE(pm))
		return (0);
	return (1);
}

/*
 * Remove the given range of addresses from the specified map.
 */
void
pmap_remove(pmap_t pm, vm_offset_t start, vm_offset_t end)
{
	struct tte *tp;
	vm_offset_t va;

	CTR3(KTR_PMAP, "pmap_remove: ctx=%#lx start=%#lx end=%#lx",
	    pm->pm_context[PCPU_GET(cpuid)], start, end);
	if (PMAP_REMOVE_DONE(pm))
		return;
	vm_page_lock_queues();
	PMAP_LOCK(pm);
	if (end - start > PMAP_TSB_THRESH) {
		tsb_foreach(pm, NULL, start, end, pmap_remove_tte);
		tlb_context_demap(pm);
	} else {
		for (va = start; va < end; va += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(pm, va)) != NULL) {
				if (!pmap_remove_tte(pm, NULL, tp, va))
					break;
			}
		}
		tlb_range_demap(pm, start, end - 1);
	}
	PMAP_UNLOCK(pm);
	vm_page_unlock_queues();
}

void
pmap_remove_all(vm_page_t m)
{
	struct pmap *pm;
	struct tte *tpn;
	struct tte *tp;
	vm_offset_t va;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	for (tp = TAILQ_FIRST(&m->md.tte_list); tp != NULL; tp = tpn) {
		tpn = TAILQ_NEXT(tp, tte_link);
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		pm = TTE_GET_PMAP(tp);
		va = TTE_GET_VA(tp);
		PMAP_LOCK(pm);
		if ((tp->tte_data & TD_WIRED) != 0)
			pm->pm_stats.wired_count--;
		if ((tp->tte_data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		if ((tp->tte_data & TD_W) != 0)
			vm_page_dirty(m);
		tp->tte_data &= ~TD_V;
		tlb_page_demap(pm, va);
		TAILQ_REMOVE(&m->md.tte_list, tp, tte_link);
		pm->pm_stats.resident_count--;
		pmap_cache_remove(m, va);
		TTE_ZERO(tp);
		PMAP_UNLOCK(pm);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
}

int
pmap_protect_tte(struct pmap *pm, struct pmap *pm2, struct tte *tp,
		 vm_offset_t va)
{
	u_long data;
	vm_page_t m;

	data = atomic_clear_long(&tp->tte_data, TD_REF | TD_SW | TD_W);
	if ((data & TD_PV) != 0) {
		m = PHYS_TO_VM_PAGE(TD_PA(data));
		if ((data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		if ((data & TD_W) != 0)
			vm_page_dirty(m);
	}
	return (1);
}

/*
 * Set the physical protection on the specified range of this map as requested.
 */
void
pmap_protect(pmap_t pm, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t va;
	struct tte *tp;

	CTR4(KTR_PMAP, "pmap_protect: ctx=%#lx sva=%#lx eva=%#lx prot=%#lx",
	    pm->pm_context[PCPU_GET(cpuid)], sva, eva, prot);

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pm, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	vm_page_lock_queues();
	PMAP_LOCK(pm);
	if (eva - sva > PMAP_TSB_THRESH) {
		tsb_foreach(pm, NULL, sva, eva, pmap_protect_tte);
		tlb_context_demap(pm);
	} else {
		for (va = sva; va < eva; va += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(pm, va)) != NULL)
				pmap_protect_tte(pm, NULL, tp, va);
		}
		tlb_range_demap(pm, sva, eva - 1);
	}
	PMAP_UNLOCK(pm);
	vm_page_unlock_queues();
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 */
void
pmap_enter(pmap_t pm, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{

	vm_page_lock_queues();
	PMAP_LOCK(pm);
	pmap_enter_locked(pm, va, m, prot, wired);
	vm_page_unlock_queues();
	PMAP_UNLOCK(pm);
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 *
 * The page queues and pmap must be locked.
 */
static void
pmap_enter_locked(pmap_t pm, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    boolean_t wired)
{
	struct tte *tp;
	vm_paddr_t pa;
	u_long data;
	int i;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pm, MA_OWNED);
	PMAP_STATS_INC(pmap_nenter);
	pa = VM_PAGE_TO_PHYS(m);

	/*
	 * If this is a fake page from the device_pager, but it covers actual
	 * physical memory, convert to the real backing page.
	 */
	if ((m->flags & PG_FICTITIOUS) != 0) {
		for (i = 0; phys_avail[i + 1] != 0; i += 2) {
			if (pa >= phys_avail[i] && pa <= phys_avail[i + 1]) {
				m = PHYS_TO_VM_PAGE(pa);
				break;
			}
		}
	}

	CTR6(KTR_PMAP,
	    "pmap_enter: ctx=%p m=%p va=%#lx pa=%#lx prot=%#x wired=%d",
	    pm->pm_context[PCPU_GET(cpuid)], m, va, pa, prot, wired);

	/*
	 * If there is an existing mapping, and the physical address has not
	 * changed, must be protection or wiring change.
	 */
	if ((tp = tsb_tte_lookup(pm, va)) != NULL && TTE_GET_PA(tp) == pa) {
		CTR0(KTR_PMAP, "pmap_enter: update");
		PMAP_STATS_INC(pmap_nenter_update);

		/*
		 * Wiring change, just update stats.
		 */
		if (wired) {
			if ((tp->tte_data & TD_WIRED) == 0) {
				tp->tte_data |= TD_WIRED;
				pm->pm_stats.wired_count++;
			}
		} else {
			if ((tp->tte_data & TD_WIRED) != 0) {
				tp->tte_data &= ~TD_WIRED;
				pm->pm_stats.wired_count--;
			}
		}

		/*
		 * Save the old bits and clear the ones we're interested in.
		 */
		data = tp->tte_data;
		tp->tte_data &= ~(TD_EXEC | TD_SW | TD_W);

		/*
		 * If we're turning off write permissions, sense modify status.
		 */
		if ((prot & VM_PROT_WRITE) != 0) {
			tp->tte_data |= TD_SW;
			if (wired) {
				tp->tte_data |= TD_W;
			}
			vm_page_flag_set(m, PG_WRITEABLE);
		} else if ((data & TD_W) != 0) {
			vm_page_dirty(m);
		}

		/*
		 * If we're turning on execute permissions, flush the icache.
		 */
		if ((prot & VM_PROT_EXECUTE) != 0) {
			if ((data & TD_EXEC) == 0) {
				icache_page_inval(pa);
			}
			tp->tte_data |= TD_EXEC;
		}

		/*
		 * Delete the old mapping.
		 */
		tlb_page_demap(pm, TTE_GET_VA(tp));
	} else {
		/*
		 * If there is an existing mapping, but its for a different
		 * phsyical address, delete the old mapping.
		 */
		if (tp != NULL) {
			CTR0(KTR_PMAP, "pmap_enter: replace");
			PMAP_STATS_INC(pmap_nenter_replace);
			pmap_remove_tte(pm, NULL, tp, va);
			tlb_page_demap(pm, va);
		} else {
			CTR0(KTR_PMAP, "pmap_enter: new");
			PMAP_STATS_INC(pmap_nenter_new);
		}

		/*
		 * Now set up the data and install the new mapping.
		 */
		data = TD_V | TD_8K | TD_PA(pa);
		if (pm == kernel_pmap)
			data |= TD_P;
		if ((prot & VM_PROT_WRITE) != 0) {
			data |= TD_SW;
			vm_page_flag_set(m, PG_WRITEABLE);
		}
		if (prot & VM_PROT_EXECUTE) {
			data |= TD_EXEC;
			icache_page_inval(pa);
		}

		/*
		 * If its wired update stats.  We also don't need reference or
		 * modify tracking for wired mappings, so set the bits now.
		 */
		if (wired) {
			pm->pm_stats.wired_count++;
			data |= TD_REF | TD_WIRED;
			if ((prot & VM_PROT_WRITE) != 0)
				data |= TD_W;
		}

		tsb_tte_enter(pm, m, va, TS_8K, data);
	}
}

/*
 * Maps a sequence of resident pages belonging to the same object.
 * The sequence begins with the given page m_start.  This page is
 * mapped at the given virtual address start.  Each subsequent page is
 * mapped at a virtual address that is offset from start by the same
 * amount as the page is offset from m_start within the object.  The
 * last page in the sequence is the page with the largest offset from
 * m_start that can be mapped at a virtual address less than the given
 * virtual address end.  Not every virtual page between start and end
 * is mapped; only those for which a resident page exists with the
 * corresponding offset from m_start are mapped.
 */
void
pmap_enter_object(pmap_t pm, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	vm_page_t m;
	vm_pindex_t diff, psize;

	psize = atop(end - start);
	m = m_start;
	PMAP_LOCK(pm);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		pmap_enter_locked(pm, start + ptoa(diff), m, prot &
		    (VM_PROT_READ | VM_PROT_EXECUTE), FALSE);
		m = TAILQ_NEXT(m, listq);
	}
	PMAP_UNLOCK(pm);
}

void
pmap_enter_quick(pmap_t pm, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{

	PMAP_LOCK(pm);
	pmap_enter_locked(pm, va, m, prot & (VM_PROT_READ | VM_PROT_EXECUTE),
	    FALSE);
	PMAP_UNLOCK(pm);
}

void
pmap_object_init_pt(pmap_t pm, vm_offset_t addr, vm_object_t object,
		    vm_pindex_t pindex, vm_size_t size)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT(object->type == OBJT_DEVICE,
	    ("pmap_object_init_pt: non-device object"));
}

/*
 * Change the wiring attribute for a map/virtual-address pair.
 * The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap_t pm, vm_offset_t va, boolean_t wired)
{
	struct tte *tp;
	u_long data;

	PMAP_LOCK(pm);
	if ((tp = tsb_tte_lookup(pm, va)) != NULL) {
		if (wired) {
			data = atomic_set_long(&tp->tte_data, TD_WIRED);
			if ((data & TD_WIRED) == 0)
				pm->pm_stats.wired_count++;
		} else {
			data = atomic_clear_long(&tp->tte_data, TD_WIRED);
			if ((data & TD_WIRED) != 0)
				pm->pm_stats.wired_count--;
		}
	}
	PMAP_UNLOCK(pm);
}

static int
pmap_copy_tte(pmap_t src_pmap, pmap_t dst_pmap, struct tte *tp, vm_offset_t va)
{
	vm_page_t m;
	u_long data;

	if ((tp->tte_data & TD_FAKE) != 0)
		return (1);
	if (tsb_tte_lookup(dst_pmap, va) == NULL) {
		data = tp->tte_data &
		    ~(TD_PV | TD_REF | TD_SW | TD_CV | TD_W);
		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
		tsb_tte_enter(dst_pmap, m, va, TS_8K, data);
	}
	return (1);
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	struct tte *tp;
	vm_offset_t va;

	if (dst_addr != src_addr)
		return;
	vm_page_lock_queues();
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}
	if (len > PMAP_TSB_THRESH) {
		tsb_foreach(src_pmap, dst_pmap, src_addr, src_addr + len,
		    pmap_copy_tte);
		tlb_context_demap(dst_pmap);
	} else {
		for (va = src_addr; va < src_addr + len; va += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(src_pmap, va)) != NULL)
				pmap_copy_tte(src_pmap, dst_pmap, tp, va);
		}
		tlb_range_demap(dst_pmap, src_addr, src_addr + len - 1);
	}
	vm_page_unlock_queues();
	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);
}

void
pmap_zero_page(vm_page_t m)
{
	struct tte *tp;
	vm_offset_t va;
	vm_paddr_t pa;

	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_zero_page: fake page"));
	PMAP_STATS_INC(pmap_nzero_page);
	pa = VM_PAGE_TO_PHYS(m);
	if (m->md.color == -1) {
		PMAP_STATS_INC(pmap_nzero_page_nc);
		aszero(ASI_PHYS_USE_EC, pa, PAGE_SIZE);
	} else if (m->md.color == DCACHE_COLOR(pa)) {
		PMAP_STATS_INC(pmap_nzero_page_c);
		va = TLB_PHYS_TO_DIRECT(pa);
		cpu_block_zero((void *)va, PAGE_SIZE);
	} else {
		PMAP_STATS_INC(pmap_nzero_page_oc);
		PMAP_LOCK(kernel_pmap);
		va = pmap_temp_map_1 + (m->md.color * PAGE_SIZE);
		tp = tsb_kvtotte(va);
		tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_CP | TD_CV | TD_W;
		tp->tte_vpn = TV_VPN(va, TS_8K);
		cpu_block_zero((void *)va, PAGE_SIZE);
		tlb_page_demap(kernel_pmap, va);
		PMAP_UNLOCK(kernel_pmap);
	}
}

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	struct tte *tp;
	vm_offset_t va;
	vm_paddr_t pa;

	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_zero_page_area: fake page"));
	KASSERT(off + size <= PAGE_SIZE, ("pmap_zero_page_area: bad off/size"));
	PMAP_STATS_INC(pmap_nzero_page_area);
	pa = VM_PAGE_TO_PHYS(m);
	if (m->md.color == -1) {
		PMAP_STATS_INC(pmap_nzero_page_area_nc);
		aszero(ASI_PHYS_USE_EC, pa + off, size);
	} else if (m->md.color == DCACHE_COLOR(pa)) {
		PMAP_STATS_INC(pmap_nzero_page_area_c);
		va = TLB_PHYS_TO_DIRECT(pa);
		bzero((void *)(va + off), size);
	} else {
		PMAP_STATS_INC(pmap_nzero_page_area_oc);
		PMAP_LOCK(kernel_pmap);
		va = pmap_temp_map_1 + (m->md.color * PAGE_SIZE);
		tp = tsb_kvtotte(va);
		tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_CP | TD_CV | TD_W;
		tp->tte_vpn = TV_VPN(va, TS_8K);
		bzero((void *)(va + off), size);
		tlb_page_demap(kernel_pmap, va);
		PMAP_UNLOCK(kernel_pmap);
	}
}

void
pmap_zero_page_idle(vm_page_t m)
{
	struct tte *tp;
	vm_offset_t va;
	vm_paddr_t pa;

	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_zero_page_idle: fake page"));
	PMAP_STATS_INC(pmap_nzero_page_idle);
	pa = VM_PAGE_TO_PHYS(m);
	if (m->md.color == -1) {
		PMAP_STATS_INC(pmap_nzero_page_idle_nc);
		aszero(ASI_PHYS_USE_EC, pa, PAGE_SIZE);
	} else if (m->md.color == DCACHE_COLOR(pa)) {
		PMAP_STATS_INC(pmap_nzero_page_idle_c);
		va = TLB_PHYS_TO_DIRECT(pa);
		cpu_block_zero((void *)va, PAGE_SIZE);
	} else {
		PMAP_STATS_INC(pmap_nzero_page_idle_oc);
		va = pmap_idle_map + (m->md.color * PAGE_SIZE);
		tp = tsb_kvtotte(va);
		tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_CP | TD_CV | TD_W;
		tp->tte_vpn = TV_VPN(va, TS_8K);
		cpu_block_zero((void *)va, PAGE_SIZE);
		tlb_page_demap(kernel_pmap, va);
	}
}

void
pmap_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t vdst;
	vm_offset_t vsrc;
	vm_paddr_t pdst;
	vm_paddr_t psrc;
	struct tte *tp;

	KASSERT((mdst->flags & PG_FICTITIOUS) == 0,
	    ("pmap_copy_page: fake dst page"));
	KASSERT((msrc->flags & PG_FICTITIOUS) == 0,
	    ("pmap_copy_page: fake src page"));
	PMAP_STATS_INC(pmap_ncopy_page);
	pdst = VM_PAGE_TO_PHYS(mdst);
	psrc = VM_PAGE_TO_PHYS(msrc);
	if (msrc->md.color == -1 && mdst->md.color == -1) {
		PMAP_STATS_INC(pmap_ncopy_page_nc);
		ascopy(ASI_PHYS_USE_EC, psrc, pdst, PAGE_SIZE);
	} else if (msrc->md.color == DCACHE_COLOR(psrc) &&
	    mdst->md.color == DCACHE_COLOR(pdst)) {
		PMAP_STATS_INC(pmap_ncopy_page_c);
		vdst = TLB_PHYS_TO_DIRECT(pdst);
		vsrc = TLB_PHYS_TO_DIRECT(psrc);
		cpu_block_copy((void *)vsrc, (void *)vdst, PAGE_SIZE);
	} else if (msrc->md.color == -1) {
		if (mdst->md.color == DCACHE_COLOR(pdst)) {
			PMAP_STATS_INC(pmap_ncopy_page_dc);
			vdst = TLB_PHYS_TO_DIRECT(pdst);
			ascopyfrom(ASI_PHYS_USE_EC, psrc, (void *)vdst,
			    PAGE_SIZE);
		} else {
			PMAP_STATS_INC(pmap_ncopy_page_doc);
			PMAP_LOCK(kernel_pmap);
			vdst = pmap_temp_map_1 + (mdst->md.color * PAGE_SIZE);
			tp = tsb_kvtotte(vdst);
			tp->tte_data =
			    TD_V | TD_8K | TD_PA(pdst) | TD_CP | TD_CV | TD_W;
			tp->tte_vpn = TV_VPN(vdst, TS_8K);
			ascopyfrom(ASI_PHYS_USE_EC, psrc, (void *)vdst,
			    PAGE_SIZE);
			tlb_page_demap(kernel_pmap, vdst);
			PMAP_UNLOCK(kernel_pmap);
		}
	} else if (mdst->md.color == -1) {
		if (msrc->md.color == DCACHE_COLOR(psrc)) {
			PMAP_STATS_INC(pmap_ncopy_page_sc);
			vsrc = TLB_PHYS_TO_DIRECT(psrc);
			ascopyto((void *)vsrc, ASI_PHYS_USE_EC, pdst,
			    PAGE_SIZE);
		} else {
			PMAP_STATS_INC(pmap_ncopy_page_soc);
			PMAP_LOCK(kernel_pmap);
			vsrc = pmap_temp_map_1 + (msrc->md.color * PAGE_SIZE);
			tp = tsb_kvtotte(vsrc);
			tp->tte_data =
			    TD_V | TD_8K | TD_PA(psrc) | TD_CP | TD_CV | TD_W;
			tp->tte_vpn = TV_VPN(vsrc, TS_8K);
			ascopyto((void *)vsrc, ASI_PHYS_USE_EC, pdst,
			    PAGE_SIZE);
			tlb_page_demap(kernel_pmap, vsrc);
			PMAP_UNLOCK(kernel_pmap);
		}
	} else {
		PMAP_STATS_INC(pmap_ncopy_page_oc);
		PMAP_LOCK(kernel_pmap);
		vdst = pmap_temp_map_1 + (mdst->md.color * PAGE_SIZE);
		tp = tsb_kvtotte(vdst);
		tp->tte_data =
		    TD_V | TD_8K | TD_PA(pdst) | TD_CP | TD_CV | TD_W;
		tp->tte_vpn = TV_VPN(vdst, TS_8K);
		vsrc = pmap_temp_map_2 + (msrc->md.color * PAGE_SIZE);
		tp = tsb_kvtotte(vsrc);
		tp->tte_data =
		    TD_V | TD_8K | TD_PA(psrc) | TD_CP | TD_CV | TD_W;
		tp->tte_vpn = TV_VPN(vsrc, TS_8K);
		cpu_block_copy((void *)vsrc, (void *)vdst, PAGE_SIZE);
		tlb_page_demap(kernel_pmap, vdst);
		tlb_page_demap(kernel_pmap, vsrc);
		PMAP_UNLOCK(kernel_pmap);
	}
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap_t pm, vm_page_t m)
{
	struct tte *tp;
	int loops;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return (FALSE);
	loops = 0;
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		if (TTE_GET_PMAP(tp) == pm)
			return (TRUE);
		if (++loops >= 16)
			break;
	}
	return (FALSE);
}

/*
 * Remove all pages from specified address space, this aids process exit
 * speeds.  This is much faster than pmap_remove n the case of running down
 * an entire address space.  Only works for the current pmap.
 */
void
pmap_remove_pages(pmap_t pm)
{
}

/*
 * Returns TRUE if the given page has a managed mapping.
 */
boolean_t
pmap_page_is_mapped(vm_page_t m)
{
	struct tte *tp;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return (FALSE);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) != 0)
			return (TRUE);
	}
	return (FALSE);
}

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 *
 *	XXX: The exact number of bits to check and clear is a matter that
 *	should be tested and standardized at some point in the future for
 *	optimal aging of shared pages.
 */

int
pmap_ts_referenced(vm_page_t m)
{
	struct tte *tpf;
	struct tte *tpn;
	struct tte *tp;
	u_long data;
	int count;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return (0);
	count = 0;
	if ((tp = TAILQ_FIRST(&m->md.tte_list)) != NULL) {
		tpf = tp;
		do {
			tpn = TAILQ_NEXT(tp, tte_link);
			TAILQ_REMOVE(&m->md.tte_list, tp, tte_link);
			TAILQ_INSERT_TAIL(&m->md.tte_list, tp, tte_link);
			if ((tp->tte_data & TD_PV) == 0)
				continue;
			data = atomic_clear_long(&tp->tte_data, TD_REF);
			if ((data & TD_REF) != 0 && ++count > 4)
				break;
		} while ((tp = tpn) != NULL && tp != tpf);
	}
	return (count);
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	struct tte *tp;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return (FALSE);
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		if ((tp->tte_data & TD_W) != 0)
			return (TRUE);
	}
	return (FALSE);
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{

	return (FALSE);
}

void
pmap_clear_modify(vm_page_t m)
{
	struct tte *tp;
	u_long data;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return;
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		data = atomic_clear_long(&tp->tte_data, TD_W);
		if ((data & TD_W) != 0)
			tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
	}
}

void
pmap_clear_reference(vm_page_t m)
{
	struct tte *tp;
	u_long data;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return;
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		data = atomic_clear_long(&tp->tte_data, TD_REF);
		if ((data & TD_REF) != 0)
			tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
	}
}

void
pmap_remove_write(vm_page_t m)
{
	struct tte *tp;
	u_long data;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0 ||
	    (m->flags & PG_WRITEABLE) == 0)
		return;
	TAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		data = atomic_clear_long(&tp->tte_data, TD_SW | TD_W);
		if ((data & TD_W) != 0) {
			vm_page_dirty(m);
			tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
		}
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
}

int
pmap_mincore(pmap_t pm, vm_offset_t addr)
{
	/* TODO; */
	return (0);
}

/*
 * Activate a user pmap.  The pmap must be activated before its address space
 * can be accessed in any way.
 */
void
pmap_activate(struct thread *td)
{
	struct vmspace *vm;
	struct pmap *pm;
	int context;

	vm = td->td_proc->p_vmspace;
	pm = vmspace_pmap(vm);

	mtx_lock_spin(&sched_lock);

	context = PCPU_GET(tlb_ctx);
	if (context == PCPU_GET(tlb_ctx_max)) {
		tlb_flush_user();
		context = PCPU_GET(tlb_ctx_min);
	}
	PCPU_SET(tlb_ctx, context + 1);

	pm->pm_context[PCPU_GET(cpuid)] = context;
	pm->pm_active |= PCPU_GET(cpumask);
	PCPU_SET(pmap, pm);

	stxa(AA_DMMU_TSB, ASI_DMMU, pm->pm_tsb);
	stxa(AA_IMMU_TSB, ASI_IMMU, pm->pm_tsb);
	stxa(AA_DMMU_PCXR, ASI_DMMU, context);
	membar(Sync);

	mtx_unlock_spin(&sched_lock);
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{

	return (va);
}
