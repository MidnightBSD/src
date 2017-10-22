/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	from: @(#)vm_page.c	7.4 (Berkeley) 5/7/91
 */

/*-
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *			GENERAL RULES ON VM_PAGE MANIPULATION
 *
 *	- A page queue lock is required when adding or removing a page from a
 *	  page queue regardless of other locks or the busy state of a page.
 *
 *		* In general, no thread besides the page daemon can acquire or
 *		  hold more than one page queue lock at a time.
 *
 *		* The page daemon can acquire and hold any pair of page queue
 *		  locks in any order.
 *
 *	- The object lock is required when inserting or removing
 *	  pages from an object (vm_page_insert() or vm_page_remove()).
 *
 */

/*
 *	Resident memory management module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/vm/vm_page.c 255626 2013-09-17 07:35:26Z kib $");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>

/*
 *	Associated with page of user-allocatable memory is a
 *	page structure.
 */

struct vm_domain vm_dom[MAXMEMDOM];
struct mtx_padalign vm_page_queue_free_mtx;

struct mtx_padalign pa_lock[PA_LOCK_COUNT];

vm_page_t vm_page_array;
long vm_page_array_size;
long first_page;
int vm_page_zero_count;

static int boot_pages = UMA_BOOT_PAGES;
TUNABLE_INT("vm.boot_pages", &boot_pages);
SYSCTL_INT(_vm, OID_AUTO, boot_pages, CTLFLAG_RD, &boot_pages, 0,
	"number of pages allocated for bootstrapping the VM system");

static int pa_tryrelock_restart;
SYSCTL_INT(_vm, OID_AUTO, tryrelock_restart, CTLFLAG_RD,
    &pa_tryrelock_restart, 0, "Number of tryrelock restarts");

static uma_zone_t fakepg_zone;

static struct vnode *vm_page_alloc_init(vm_page_t m);
static void vm_page_cache_turn_free(vm_page_t m);
static void vm_page_clear_dirty_mask(vm_page_t m, vm_page_bits_t pagebits);
static void vm_page_enqueue(int queue, vm_page_t m);
static void vm_page_init_fakepg(void *dummy);
static int vm_page_insert_after(vm_page_t m, vm_object_t object,
    vm_pindex_t pindex, vm_page_t mpred);
static void vm_page_insert_radixdone(vm_page_t m, vm_object_t object,
    vm_page_t mpred);

SYSINIT(vm_page, SI_SUB_VM, SI_ORDER_SECOND, vm_page_init_fakepg, NULL);

static void
vm_page_init_fakepg(void *dummy)
{

	fakepg_zone = uma_zcreate("fakepg", sizeof(struct vm_page), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE | UMA_ZONE_VM); 
}

/* Make sure that u_long is at least 64 bits when PAGE_SIZE is 32K. */
#if PAGE_SIZE == 32768
#ifdef CTASSERT
CTASSERT(sizeof(u_long) >= 8);
#endif
#endif

/*
 * Try to acquire a physical address lock while a pmap is locked.  If we
 * fail to trylock we unlock and lock the pmap directly and cache the
 * locked pa in *locked.  The caller should then restart their loop in case
 * the virtual to physical mapping has changed.
 */
int
vm_page_pa_tryrelock(pmap_t pmap, vm_paddr_t pa, vm_paddr_t *locked)
{
	vm_paddr_t lockpa;

	lockpa = *locked;
	*locked = pa;
	if (lockpa) {
		PA_LOCK_ASSERT(lockpa, MA_OWNED);
		if (PA_LOCKPTR(pa) == PA_LOCKPTR(lockpa))
			return (0);
		PA_UNLOCK(lockpa);
	}
	if (PA_TRYLOCK(pa))
		return (0);
	PMAP_UNLOCK(pmap);
	atomic_add_int(&pa_tryrelock_restart, 1);
	PA_LOCK(pa);
	PMAP_LOCK(pmap);
	return (EAGAIN);
}

/*
 *	vm_set_page_size:
 *
 *	Sets the page size, perhaps based upon the memory
 *	size.  Must be called before any use of page-size
 *	dependent functions.
 */
void
vm_set_page_size(void)
{
	if (cnt.v_page_size == 0)
		cnt.v_page_size = PAGE_SIZE;
	if (((cnt.v_page_size - 1) & cnt.v_page_size) != 0)
		panic("vm_set_page_size: page size not a power of two");
}

/*
 *	vm_page_blacklist_lookup:
 *
 *	See if a physical address in this page has been listed
 *	in the blacklist tunable.  Entries in the tunable are
 *	separated by spaces or commas.  If an invalid integer is
 *	encountered then the rest of the string is skipped.
 */
static int
vm_page_blacklist_lookup(char *list, vm_paddr_t pa)
{
	vm_paddr_t bad;
	char *cp, *pos;

	for (pos = list; *pos != '\0'; pos = cp) {
		bad = strtoq(pos, &cp, 0);
		if (*cp != '\0') {
			if (*cp == ' ' || *cp == ',') {
				cp++;
				if (cp == pos)
					continue;
			} else
				break;
		}
		if (pa == trunc_page(bad))
			return (1);
	}
	return (0);
}

static void
vm_page_domain_init(struct vm_domain *vmd)
{
	struct vm_pagequeue *pq;
	int i;

	*__DECONST(char **, &vmd->vmd_pagequeues[PQ_INACTIVE].pq_name) =
	    "vm inactive pagequeue";
	*__DECONST(int **, &vmd->vmd_pagequeues[PQ_INACTIVE].pq_vcnt) =
	    &cnt.v_inactive_count;
	*__DECONST(char **, &vmd->vmd_pagequeues[PQ_ACTIVE].pq_name) =
	    "vm active pagequeue";
	*__DECONST(int **, &vmd->vmd_pagequeues[PQ_ACTIVE].pq_vcnt) =
	    &cnt.v_active_count;
	vmd->vmd_page_count = 0;
	vmd->vmd_free_count = 0;
	vmd->vmd_segs = 0;
	vmd->vmd_oom = FALSE;
	vmd->vmd_pass = 0;
	for (i = 0; i < PQ_COUNT; i++) {
		pq = &vmd->vmd_pagequeues[i];
		TAILQ_INIT(&pq->pq_pl);
		mtx_init(&pq->pq_mutex, pq->pq_name, "vm pagequeue",
		    MTX_DEF | MTX_DUPOK);
	}
}

/*
 *	vm_page_startup:
 *
 *	Initializes the resident memory module.
 *
 *	Allocates memory for the page cells, and
 *	for the object/offset-to-page hash table headers.
 *	Each page cell is initialized and placed on the free list.
 */
vm_offset_t
vm_page_startup(vm_offset_t vaddr)
{
	vm_offset_t mapped;
	vm_paddr_t page_range;
	vm_paddr_t new_end;
	int i;
	vm_paddr_t pa;
	vm_paddr_t last_pa;
	char *list;

	/* the biggest memory array is the second group of pages */
	vm_paddr_t end;
	vm_paddr_t biggestsize;
	vm_paddr_t low_water, high_water;
	int biggestone;

	biggestsize = 0;
	biggestone = 0;
	vaddr = round_page(vaddr);

	for (i = 0; phys_avail[i + 1]; i += 2) {
		phys_avail[i] = round_page(phys_avail[i]);
		phys_avail[i + 1] = trunc_page(phys_avail[i + 1]);
	}

	low_water = phys_avail[0];
	high_water = phys_avail[1];

	for (i = 0; phys_avail[i + 1]; i += 2) {
		vm_paddr_t size = phys_avail[i + 1] - phys_avail[i];

		if (size > biggestsize) {
			biggestone = i;
			biggestsize = size;
		}
		if (phys_avail[i] < low_water)
			low_water = phys_avail[i];
		if (phys_avail[i + 1] > high_water)
			high_water = phys_avail[i + 1];
	}

#ifdef XEN
	low_water = 0;
#endif	

	end = phys_avail[biggestone+1];

	/*
	 * Initialize the page and queue locks.
	 */
	mtx_init(&vm_page_queue_free_mtx, "vm page free queue", NULL, MTX_DEF);
	for (i = 0; i < PA_LOCK_COUNT; i++)
		mtx_init(&pa_lock[i], "vm page", NULL, MTX_DEF);
	for (i = 0; i < vm_ndomains; i++)
		vm_page_domain_init(&vm_dom[i]);

	/*
	 * Allocate memory for use when boot strapping the kernel memory
	 * allocator.
	 */
	new_end = end - (boot_pages * UMA_SLAB_SIZE);
	new_end = trunc_page(new_end);
	mapped = pmap_map(&vaddr, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	bzero((void *)mapped, end - new_end);
	uma_startup((void *)mapped, boot_pages);

#if defined(__amd64__) || defined(__i386__) || defined(__arm__) || \
    defined(__mips__)
	/*
	 * Allocate a bitmap to indicate that a random physical page
	 * needs to be included in a minidump.
	 *
	 * The amd64 port needs this to indicate which direct map pages
	 * need to be dumped, via calls to dump_add_page()/dump_drop_page().
	 *
	 * However, i386 still needs this workspace internally within the
	 * minidump code.  In theory, they are not needed on i386, but are
	 * included should the sf_buf code decide to use them.
	 */
	last_pa = 0;
	for (i = 0; dump_avail[i + 1] != 0; i += 2)
		if (dump_avail[i + 1] > last_pa)
			last_pa = dump_avail[i + 1];
	page_range = last_pa / PAGE_SIZE;
	vm_page_dump_size = round_page(roundup2(page_range, NBBY) / NBBY);
	new_end -= vm_page_dump_size;
	vm_page_dump = (void *)(uintptr_t)pmap_map(&vaddr, new_end,
	    new_end + vm_page_dump_size, VM_PROT_READ | VM_PROT_WRITE);
	bzero((void *)vm_page_dump, vm_page_dump_size);
#endif
#ifdef __amd64__
	/*
	 * Request that the physical pages underlying the message buffer be
	 * included in a crash dump.  Since the message buffer is accessed
	 * through the direct map, they are not automatically included.
	 */
	pa = DMAP_TO_PHYS((vm_offset_t)msgbufp->msg_ptr);
	last_pa = pa + round_page(msgbufsize);
	while (pa < last_pa) {
		dump_add_page(pa);
		pa += PAGE_SIZE;
	}
#endif
	/*
	 * Compute the number of pages of memory that will be available for
	 * use (taking into account the overhead of a page structure per
	 * page).
	 */
	first_page = low_water / PAGE_SIZE;
#ifdef VM_PHYSSEG_SPARSE
	page_range = 0;
	for (i = 0; phys_avail[i + 1] != 0; i += 2)
		page_range += atop(phys_avail[i + 1] - phys_avail[i]);
#elif defined(VM_PHYSSEG_DENSE)
	page_range = high_water / PAGE_SIZE - first_page;
#else
#error "Either VM_PHYSSEG_DENSE or VM_PHYSSEG_SPARSE must be defined."
#endif
	end = new_end;

	/*
	 * Reserve an unmapped guard page to trap access to vm_page_array[-1].
	 */
	vaddr += PAGE_SIZE;

	/*
	 * Initialize the mem entry structures now, and put them in the free
	 * queue.
	 */
	new_end = trunc_page(end - page_range * sizeof(struct vm_page));
	mapped = pmap_map(&vaddr, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	vm_page_array = (vm_page_t) mapped;
#if VM_NRESERVLEVEL > 0
	/*
	 * Allocate memory for the reservation management system's data
	 * structures.
	 */
	new_end = vm_reserv_startup(&vaddr, new_end, high_water);
#endif
#if defined(__amd64__) || defined(__mips__)
	/*
	 * pmap_map on amd64 and mips can come out of the direct-map, not kvm
	 * like i386, so the pages must be tracked for a crashdump to include
	 * this data.  This includes the vm_page_array and the early UMA
	 * bootstrap pages.
	 */
	for (pa = new_end; pa < phys_avail[biggestone + 1]; pa += PAGE_SIZE)
		dump_add_page(pa);
#endif	
	phys_avail[biggestone + 1] = new_end;

	/*
	 * Clear all of the page structures
	 */
	bzero((caddr_t) vm_page_array, page_range * sizeof(struct vm_page));
	for (i = 0; i < page_range; i++)
		vm_page_array[i].order = VM_NFREEORDER;
	vm_page_array_size = page_range;

	/*
	 * Initialize the physical memory allocator.
	 */
	vm_phys_init();

	/*
	 * Add every available physical page that is not blacklisted to
	 * the free lists.
	 */
	cnt.v_page_count = 0;
	cnt.v_free_count = 0;
	list = getenv("vm.blacklist");
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		pa = phys_avail[i];
		last_pa = phys_avail[i + 1];
		while (pa < last_pa) {
			if (list != NULL &&
			    vm_page_blacklist_lookup(list, pa))
				printf("Skipping page with pa 0x%jx\n",
				    (uintmax_t)pa);
			else
				vm_phys_add_page(pa);
			pa += PAGE_SIZE;
		}
	}
	freeenv(list);
#if VM_NRESERVLEVEL > 0
	/*
	 * Initialize the reservation management system.
	 */
	vm_reserv_init();
#endif
	return (vaddr);
}

void
vm_page_reference(vm_page_t m)
{

	vm_page_aflag_set(m, PGA_REFERENCED);
}

/*
 *	vm_page_busy_downgrade:
 *
 *	Downgrade an exclusive busy page into a single shared busy page.
 */
void
vm_page_busy_downgrade(vm_page_t m)
{
	u_int x;

	vm_page_assert_xbusied(m);

	for (;;) {
		x = m->busy_lock;
		x &= VPB_BIT_WAITERS;
		if (atomic_cmpset_rel_int(&m->busy_lock,
		    VPB_SINGLE_EXCLUSIVER | x, VPB_SHARERS_WORD(1) | x))
			break;
	}
}

/*
 *	vm_page_sbusied:
 *
 *	Return a positive value if the page is shared busied, 0 otherwise.
 */
int
vm_page_sbusied(vm_page_t m)
{
	u_int x;

	x = m->busy_lock;
	return ((x & VPB_BIT_SHARED) != 0 && x != VPB_UNBUSIED);
}

/*
 *	vm_page_sunbusy:
 *
 *	Shared unbusy a page.
 */
void
vm_page_sunbusy(vm_page_t m)
{
	u_int x;

	vm_page_assert_sbusied(m);

	for (;;) {
		x = m->busy_lock;
		if (VPB_SHARERS(x) > 1) {
			if (atomic_cmpset_int(&m->busy_lock, x,
			    x - VPB_ONE_SHARER))
				break;
			continue;
		}
		if ((x & VPB_BIT_WAITERS) == 0) {
			KASSERT(x == VPB_SHARERS_WORD(1),
			    ("vm_page_sunbusy: invalid lock state"));
			if (atomic_cmpset_int(&m->busy_lock,
			    VPB_SHARERS_WORD(1), VPB_UNBUSIED))
				break;
			continue;
		}
		KASSERT(x == (VPB_SHARERS_WORD(1) | VPB_BIT_WAITERS),
		    ("vm_page_sunbusy: invalid lock state for waiters"));

		vm_page_lock(m);
		if (!atomic_cmpset_int(&m->busy_lock, x, VPB_UNBUSIED)) {
			vm_page_unlock(m);
			continue;
		}
		wakeup(m);
		vm_page_unlock(m);
		break;
	}
}

/*
 *	vm_page_busy_sleep:
 *
 *	Sleep and release the page lock, using the page pointer as wchan.
 *	This is used to implement the hard-path of busying mechanism.
 *
 *	The given page must be locked.
 */
void
vm_page_busy_sleep(vm_page_t m, const char *wmesg)
{
	u_int x;

	vm_page_lock_assert(m, MA_OWNED);

	x = m->busy_lock;
	if (x == VPB_UNBUSIED) {
		vm_page_unlock(m);
		return;
	}
	if ((x & VPB_BIT_WAITERS) == 0 &&
	    !atomic_cmpset_int(&m->busy_lock, x, x | VPB_BIT_WAITERS)) {
		vm_page_unlock(m);
		return;
	}
	msleep(m, vm_page_lockptr(m), PVM | PDROP, wmesg, 0);
}

/*
 *	vm_page_trysbusy:
 *
 *	Try to shared busy a page.
 *	If the operation succeeds 1 is returned otherwise 0.
 *	The operation never sleeps.
 */
int
vm_page_trysbusy(vm_page_t m)
{
	u_int x;

	for (;;) {
		x = m->busy_lock;
		if ((x & VPB_BIT_SHARED) == 0)
			return (0);
		if (atomic_cmpset_acq_int(&m->busy_lock, x, x + VPB_ONE_SHARER))
			return (1);
	}
}

/*
 *	vm_page_xunbusy_hard:
 *
 *	Called after the first try the exclusive unbusy of a page failed.
 *	It is assumed that the waiters bit is on.
 */
void
vm_page_xunbusy_hard(vm_page_t m)
{

	vm_page_assert_xbusied(m);

	vm_page_lock(m);
	atomic_store_rel_int(&m->busy_lock, VPB_UNBUSIED);
	wakeup(m);
	vm_page_unlock(m);
}

/*
 *	vm_page_flash:
 *
 *	Wakeup anyone waiting for the page.
 *	The ownership bits do not change.
 *
 *	The given page must be locked.
 */
void
vm_page_flash(vm_page_t m)
{
	u_int x;

	vm_page_lock_assert(m, MA_OWNED);

	for (;;) {
		x = m->busy_lock;
		if ((x & VPB_BIT_WAITERS) == 0)
			return;
		if (atomic_cmpset_int(&m->busy_lock, x,
		    x & (~VPB_BIT_WAITERS)))
			break;
	}
	wakeup(m);
}

/*
 * Keep page from being freed by the page daemon
 * much of the same effect as wiring, except much lower
 * overhead and should be used only for *very* temporary
 * holding ("wiring").
 */
void
vm_page_hold(vm_page_t mem)
{

	vm_page_lock_assert(mem, MA_OWNED);
        mem->hold_count++;
}

void
vm_page_unhold(vm_page_t mem)
{

	vm_page_lock_assert(mem, MA_OWNED);
	KASSERT(mem->hold_count >= 1, ("vm_page_unhold: hold count < 0!!!"));
	--mem->hold_count;
	if (mem->hold_count == 0 && (mem->flags & PG_UNHOLDFREE) != 0)
		vm_page_free_toq(mem);
}

/*
 *	vm_page_unhold_pages:
 *
 *	Unhold each of the pages that is referenced by the given array.
 */ 
void
vm_page_unhold_pages(vm_page_t *ma, int count)
{
	struct mtx *mtx, *new_mtx;

	mtx = NULL;
	for (; count != 0; count--) {
		/*
		 * Avoid releasing and reacquiring the same page lock.
		 */
		new_mtx = vm_page_lockptr(*ma);
		if (mtx != new_mtx) {
			if (mtx != NULL)
				mtx_unlock(mtx);
			mtx = new_mtx;
			mtx_lock(mtx);
		}
		vm_page_unhold(*ma);
		ma++;
	}
	if (mtx != NULL)
		mtx_unlock(mtx);
}

vm_page_t
PHYS_TO_VM_PAGE(vm_paddr_t pa)
{
	vm_page_t m;

#ifdef VM_PHYSSEG_SPARSE
	m = vm_phys_paddr_to_vm_page(pa);
	if (m == NULL)
		m = vm_phys_fictitious_to_vm_page(pa);
	return (m);
#elif defined(VM_PHYSSEG_DENSE)
	long pi;

	pi = atop(pa);
	if (pi >= first_page && (pi - first_page) < vm_page_array_size) {
		m = &vm_page_array[pi - first_page];
		return (m);
	}
	return (vm_phys_fictitious_to_vm_page(pa));
#else
#error "Either VM_PHYSSEG_DENSE or VM_PHYSSEG_SPARSE must be defined."
#endif
}

/*
 *	vm_page_getfake:
 *
 *	Create a fictitious page with the specified physical address and
 *	memory attribute.  The memory attribute is the only the machine-
 *	dependent aspect of a fictitious page that must be initialized.
 */
vm_page_t
vm_page_getfake(vm_paddr_t paddr, vm_memattr_t memattr)
{
	vm_page_t m;

	m = uma_zalloc(fakepg_zone, M_WAITOK | M_ZERO);
	vm_page_initfake(m, paddr, memattr);
	return (m);
}

void
vm_page_initfake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr)
{

	if ((m->flags & PG_FICTITIOUS) != 0) {
		/*
		 * The page's memattr might have changed since the
		 * previous initialization.  Update the pmap to the
		 * new memattr.
		 */
		goto memattr;
	}
	m->phys_addr = paddr;
	m->queue = PQ_NONE;
	/* Fictitious pages don't use "segind". */
	m->flags = PG_FICTITIOUS;
	/* Fictitious pages don't use "order" or "pool". */
	m->oflags = VPO_UNMANAGED;
	m->busy_lock = VPB_SINGLE_EXCLUSIVER;
	m->wire_count = 1;
	pmap_page_init(m);
memattr:
	pmap_page_set_memattr(m, memattr);
}

/*
 *	vm_page_putfake:
 *
 *	Release a fictitious page.
 */
void
vm_page_putfake(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) != 0, ("managed %p", m));
	KASSERT((m->flags & PG_FICTITIOUS) != 0,
	    ("vm_page_putfake: bad page %p", m));
	uma_zfree(fakepg_zone, m);
}

/*
 *	vm_page_updatefake:
 *
 *	Update the given fictitious page to the specified physical address and
 *	memory attribute.
 */
void
vm_page_updatefake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr)
{

	KASSERT((m->flags & PG_FICTITIOUS) != 0,
	    ("vm_page_updatefake: bad page %p", m));
	m->phys_addr = paddr;
	pmap_page_set_memattr(m, memattr);
}

/*
 *	vm_page_free:
 *
 *	Free a page.
 */
void
vm_page_free(vm_page_t m)
{

	m->flags &= ~PG_ZERO;
	vm_page_free_toq(m);
}

/*
 *	vm_page_free_zero:
 *
 *	Free a page to the zerod-pages queue
 */
void
vm_page_free_zero(vm_page_t m)
{

	m->flags |= PG_ZERO;
	vm_page_free_toq(m);
}

/*
 * Unbusy and handle the page queueing for a page from the VOP_GETPAGES()
 * array which is not the request page.
 */
void
vm_page_readahead_finish(vm_page_t m)
{

	if (m->valid != 0) {
		/*
		 * Since the page is not the requested page, whether
		 * it should be activated or deactivated is not
		 * obvious.  Empirical results have shown that
		 * deactivating the page is usually the best choice,
		 * unless the page is wanted by another thread.
		 */
		vm_page_lock(m);
		if ((m->busy_lock & VPB_BIT_WAITERS) != 0)
			vm_page_activate(m);
		else
			vm_page_deactivate(m);
		vm_page_unlock(m);
		vm_page_xunbusy(m);
	} else {
		/*
		 * Free the completely invalid page.  Such page state
		 * occurs due to the short read operation which did
		 * not covered our page at all, or in case when a read
		 * error happens.
		 */
		vm_page_lock(m);
		vm_page_free(m);
		vm_page_unlock(m);
	}
}

/*
 *	vm_page_sleep_if_busy:
 *
 *	Sleep and release the page queues lock if the page is busied.
 *	Returns TRUE if the thread slept.
 *
 *	The given page must be unlocked and object containing it must
 *	be locked.
 */
int
vm_page_sleep_if_busy(vm_page_t m, const char *msg)
{
	vm_object_t obj;

	vm_page_lock_assert(m, MA_NOTOWNED);
	VM_OBJECT_ASSERT_WLOCKED(m->object);

	if (vm_page_busied(m)) {
		/*
		 * The page-specific object must be cached because page
		 * identity can change during the sleep, causing the
		 * re-lock of a different object.
		 * It is assumed that a reference to the object is already
		 * held by the callers.
		 */
		obj = m->object;
		vm_page_lock(m);
		VM_OBJECT_WUNLOCK(obj);
		vm_page_busy_sleep(m, msg);
		VM_OBJECT_WLOCK(obj);
		return (TRUE);
	}
	return (FALSE);
}

/*
 *	vm_page_dirty_KBI:		[ internal use only ]
 *
 *	Set all bits in the page's dirty field.
 *
 *	The object containing the specified page must be locked if the
 *	call is made from the machine-independent layer.
 *
 *	See vm_page_clear_dirty_mask().
 *
 *	This function should only be called by vm_page_dirty().
 */
void
vm_page_dirty_KBI(vm_page_t m)
{

	/* These assertions refer to this operation by its public name. */
	KASSERT((m->flags & PG_CACHED) == 0,
	    ("vm_page_dirty: page in cache!"));
	KASSERT(!VM_PAGE_IS_FREE(m),
	    ("vm_page_dirty: page is free!"));
	KASSERT(m->valid == VM_PAGE_BITS_ALL,
	    ("vm_page_dirty: page is invalid!"));
	m->dirty = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object and object list.
 *
 *	The object must be locked.
 */
int
vm_page_insert(vm_page_t m, vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t mpred;

	VM_OBJECT_ASSERT_WLOCKED(object);
	mpred = vm_radix_lookup_le(&object->rtree, pindex);
	return (vm_page_insert_after(m, object, pindex, mpred));
}

/*
 *	vm_page_insert_after:
 *
 *	Inserts the page "m" into the specified object at offset "pindex".
 *
 *	The page "mpred" must immediately precede the offset "pindex" within
 *	the specified object.
 *
 *	The object must be locked.
 */
static int
vm_page_insert_after(vm_page_t m, vm_object_t object, vm_pindex_t pindex,
    vm_page_t mpred)
{
	vm_pindex_t sidx;
	vm_object_t sobj;
	vm_page_t msucc;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(m->object == NULL,
	    ("vm_page_insert_after: page already inserted"));
	if (mpred != NULL) {
		KASSERT(mpred->object == object,
		    ("vm_page_insert_after: object doesn't contain mpred"));
		KASSERT(mpred->pindex < pindex,
		    ("vm_page_insert_after: mpred doesn't precede pindex"));
		msucc = TAILQ_NEXT(mpred, listq);
	} else
		msucc = TAILQ_FIRST(&object->memq);
	if (msucc != NULL)
		KASSERT(msucc->pindex > pindex,
		    ("vm_page_insert_after: msucc doesn't succeed pindex"));

	/*
	 * Record the object/offset pair in this page
	 */
	sobj = m->object;
	sidx = m->pindex;
	m->object = object;
	m->pindex = pindex;

	/*
	 * Now link into the object's ordered list of backed pages.
	 */
	if (vm_radix_insert(&object->rtree, m)) {
		m->object = sobj;
		m->pindex = sidx;
		return (1);
	}
	vm_page_insert_radixdone(m, object, mpred);
	return (0);
}

/*
 *	vm_page_insert_radixdone:
 *
 *	Complete page "m" insertion into the specified object after the
 *	radix trie hooking.
 *
 *	The page "mpred" must precede the offset "m->pindex" within the
 *	specified object.
 *
 *	The object must be locked.
 */
static void
vm_page_insert_radixdone(vm_page_t m, vm_object_t object, vm_page_t mpred)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object != NULL && m->object == object,
	    ("vm_page_insert_radixdone: page %p has inconsistent object", m));
	if (mpred != NULL) {
		KASSERT(mpred->object == object,
		    ("vm_page_insert_after: object doesn't contain mpred"));
		KASSERT(mpred->pindex < m->pindex,
		    ("vm_page_insert_after: mpred doesn't precede pindex"));
	}

	if (mpred != NULL)
		TAILQ_INSERT_AFTER(&object->memq, mpred, m, listq);
	else
		TAILQ_INSERT_HEAD(&object->memq, m, listq);

	/*
	 * Show that the object has one more resident page.
	 */
	object->resident_page_count++;

	/*
	 * Hold the vnode until the last page is released.
	 */
	if (object->resident_page_count == 1 && object->type == OBJT_VNODE)
		vhold(object->handle);

	/*
	 * Since we are inserting a new and possibly dirty page,
	 * update the object's OBJ_MIGHTBEDIRTY flag.
	 */
	if (pmap_page_is_write_mapped(m))
		vm_object_set_writeable_dirty(object);
}

/*
 *	vm_page_remove:
 *
 *	Removes the given mem entry from the object/offset-page
 *	table and the object page list, but do not invalidate/terminate
 *	the backing store.
 *
 *	The object must be locked.  The page must be locked if it is managed.
 */
void
vm_page_remove(vm_page_t m)
{
	vm_object_t object;
	boolean_t lockacq;

	if ((m->oflags & VPO_UNMANAGED) == 0)
		vm_page_lock_assert(m, MA_OWNED);
	if ((object = m->object) == NULL)
		return;
	VM_OBJECT_ASSERT_WLOCKED(object);
	if (vm_page_xbusied(m)) {
		lockacq = FALSE;
		if ((m->oflags & VPO_UNMANAGED) != 0 &&
		    !mtx_owned(vm_page_lockptr(m))) {
			lockacq = TRUE;
			vm_page_lock(m);
		}
		vm_page_flash(m);
		atomic_store_rel_int(&m->busy_lock, VPB_UNBUSIED);
		if (lockacq)
			vm_page_unlock(m);
	}

	/*
	 * Now remove from the object's list of backed pages.
	 */
	vm_radix_remove(&object->rtree, m->pindex);
	TAILQ_REMOVE(&object->memq, m, listq);

	/*
	 * And show that the object has one fewer resident page.
	 */
	object->resident_page_count--;

	/*
	 * The vnode may now be recycled.
	 */
	if (object->resident_page_count == 0 && object->type == OBJT_VNODE)
		vdrop(object->handle);

	m->object = NULL;
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 */
vm_page_t
vm_page_lookup(vm_object_t object, vm_pindex_t pindex)
{

	VM_OBJECT_ASSERT_LOCKED(object);
	return (vm_radix_lookup(&object->rtree, pindex));
}

/*
 *	vm_page_find_least:
 *
 *	Returns the page associated with the object with least pindex
 *	greater than or equal to the parameter pindex, or NULL.
 *
 *	The object must be locked.
 */
vm_page_t
vm_page_find_least(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	VM_OBJECT_ASSERT_LOCKED(object);
	if ((m = TAILQ_FIRST(&object->memq)) != NULL && m->pindex < pindex)
		m = vm_radix_lookup_ge(&object->rtree, pindex);
	return (m);
}

/*
 * Returns the given page's successor (by pindex) within the object if it is
 * resident; if none is found, NULL is returned.
 *
 * The object must be locked.
 */
vm_page_t
vm_page_next(vm_page_t m)
{
	vm_page_t next;

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if ((next = TAILQ_NEXT(m, listq)) != NULL &&
	    next->pindex != m->pindex + 1)
		next = NULL;
	return (next);
}

/*
 * Returns the given page's predecessor (by pindex) within the object if it is
 * resident; if none is found, NULL is returned.
 *
 * The object must be locked.
 */
vm_page_t
vm_page_prev(vm_page_t m)
{
	vm_page_t prev;

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if ((prev = TAILQ_PREV(m, pglist, listq)) != NULL &&
	    prev->pindex != m->pindex - 1)
		prev = NULL;
	return (prev);
}

/*
 * Uses the page mnew as a replacement for an existing page at index
 * pindex which must be already present in the object.
 *
 * The existing page must not be on a paging queue.
 */
vm_page_t
vm_page_replace(vm_page_t mnew, vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t mold, mpred;

	VM_OBJECT_ASSERT_WLOCKED(object);

	/*
	 * This function mostly follows vm_page_insert() and
	 * vm_page_remove() without the radix, object count and vnode
	 * dance.  Double check such functions for more comments.
	 */
	mpred = vm_radix_lookup(&object->rtree, pindex);
	KASSERT(mpred != NULL,
	    ("vm_page_replace: replacing page not present with pindex"));
	mpred = TAILQ_PREV(mpred, respgs, listq);
	if (mpred != NULL)
		KASSERT(mpred->pindex < pindex,
		    ("vm_page_insert_after: mpred doesn't precede pindex"));

	mnew->object = object;
	mnew->pindex = pindex;
	mold = vm_radix_replace(&object->rtree, mnew, pindex);
	KASSERT(mold->queue == PQ_NONE,
	    ("vm_page_replace: mold is on a paging queue"));

	/* Detach the old page from the resident tailq. */
	TAILQ_REMOVE(&object->memq, mold, listq);

	mold->object = NULL;
	vm_page_xunbusy(mold);

	/* Insert the new page in the resident tailq. */
	if (mpred != NULL)
		TAILQ_INSERT_AFTER(&object->memq, mpred, mnew, listq);
	else
		TAILQ_INSERT_HEAD(&object->memq, mnew, listq);
	if (pmap_page_is_write_mapped(mnew))
		vm_object_set_writeable_dirty(object);
	return (mold);
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	Note: swap associated with the page must be invalidated by the move.  We
 *	      have to do this for several reasons:  (1) we aren't freeing the
 *	      page, (2) we are dirtying the page, (3) the VM system is probably
 *	      moving the page from object A to B, and will then later move
 *	      the backing store from A to B and we can't have a conflict.
 *
 *	Note: we *always* dirty the page.  It is necessary both for the
 *	      fact that we moved it, and because we may be invalidating
 *	      swap.  If the page is on the cache, we have to deactivate it
 *	      or vm_page_dirty() will panic.  Dirty pages are not allowed
 *	      on the cache.
 *
 *	The objects must be locked.
 */
int
vm_page_rename(vm_page_t m, vm_object_t new_object, vm_pindex_t new_pindex)
{
	vm_page_t mpred;
	vm_pindex_t opidx;

	VM_OBJECT_ASSERT_WLOCKED(new_object);

	mpred = vm_radix_lookup_le(&new_object->rtree, new_pindex);
	KASSERT(mpred == NULL || mpred->pindex != new_pindex,
	    ("vm_page_rename: pindex already renamed"));

	/*
	 * Create a custom version of vm_page_insert() which does not depend
	 * by m_prev and can cheat on the implementation aspects of the
	 * function.
	 */
	opidx = m->pindex;
	m->pindex = new_pindex;
	if (vm_radix_insert(&new_object->rtree, m)) {
		m->pindex = opidx;
		return (1);
	}

	/*
	 * The operation cannot fail anymore.  The removal must happen before
	 * the listq iterator is tainted.
	 */
	m->pindex = opidx;
	vm_page_lock(m);
	vm_page_remove(m);

	/* Return back to the new pindex to complete vm_page_insert(). */
	m->pindex = new_pindex;
	m->object = new_object;
	vm_page_unlock(m);
	vm_page_insert_radixdone(m, new_object, mpred);
	vm_page_dirty(m);
	return (0);
}

/*
 *	Convert all of the given object's cached pages that have a
 *	pindex within the given range into free pages.  If the value
 *	zero is given for "end", then the range's upper bound is
 *	infinity.  If the given object is backed by a vnode and it
 *	transitions from having one or more cached pages to none, the
 *	vnode's hold count is reduced. 
 */
void
vm_page_cache_free(vm_object_t object, vm_pindex_t start, vm_pindex_t end)
{
	vm_page_t m;
	boolean_t empty;

	mtx_lock(&vm_page_queue_free_mtx);
	if (__predict_false(vm_radix_is_empty(&object->cache))) {
		mtx_unlock(&vm_page_queue_free_mtx);
		return;
	}
	while ((m = vm_radix_lookup_ge(&object->cache, start)) != NULL) {
		if (end != 0 && m->pindex >= end)
			break;
		vm_radix_remove(&object->cache, m->pindex);
		vm_page_cache_turn_free(m);
	}
	empty = vm_radix_is_empty(&object->cache);
	mtx_unlock(&vm_page_queue_free_mtx);
	if (object->type == OBJT_VNODE && empty)
		vdrop(object->handle);
}

/*
 *	Returns the cached page that is associated with the given
 *	object and offset.  If, however, none exists, returns NULL.
 *
 *	The free page queue must be locked.
 */
static inline vm_page_t
vm_page_cache_lookup(vm_object_t object, vm_pindex_t pindex)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	return (vm_radix_lookup(&object->cache, pindex));
}

/*
 *	Remove the given cached page from its containing object's
 *	collection of cached pages.
 *
 *	The free page queue must be locked.
 */
static void
vm_page_cache_remove(vm_page_t m)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	KASSERT((m->flags & PG_CACHED) != 0,
	    ("vm_page_cache_remove: page %p is not cached", m));
	vm_radix_remove(&m->object->cache, m->pindex);
	m->object = NULL;
	cnt.v_cache_count--;
}

/*
 *	Transfer all of the cached pages with offset greater than or
 *	equal to 'offidxstart' from the original object's cache to the
 *	new object's cache.  However, any cached pages with offset
 *	greater than or equal to the new object's size are kept in the
 *	original object.  Initially, the new object's cache must be
 *	empty.  Offset 'offidxstart' in the original object must
 *	correspond to offset zero in the new object.
 *
 *	The new object must be locked.
 */
void
vm_page_cache_transfer(vm_object_t orig_object, vm_pindex_t offidxstart,
    vm_object_t new_object)
{
	vm_page_t m;

	/*
	 * Insertion into an object's collection of cached pages
	 * requires the object to be locked.  In contrast, removal does
	 * not.
	 */
	VM_OBJECT_ASSERT_WLOCKED(new_object);
	KASSERT(vm_radix_is_empty(&new_object->cache),
	    ("vm_page_cache_transfer: object %p has cached pages",
	    new_object));
	mtx_lock(&vm_page_queue_free_mtx);
	while ((m = vm_radix_lookup_ge(&orig_object->cache,
	    offidxstart)) != NULL) {
		/*
		 * Transfer all of the pages with offset greater than or
		 * equal to 'offidxstart' from the original object's
		 * cache to the new object's cache.
		 */
		if ((m->pindex - offidxstart) >= new_object->size)
			break;
		vm_radix_remove(&orig_object->cache, m->pindex);
		/* Update the page's object and offset. */
		m->object = new_object;
		m->pindex -= offidxstart;
		if (vm_radix_insert(&new_object->cache, m))
			vm_page_cache_turn_free(m);
	}
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 *	Returns TRUE if a cached page is associated with the given object and
 *	offset, and FALSE otherwise.
 *
 *	The object must be locked.
 */
boolean_t
vm_page_is_cached(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	/*
	 * Insertion into an object's collection of cached pages requires the
	 * object to be locked.  Therefore, if the object is locked and the
	 * object's collection is empty, there is no need to acquire the free
	 * page queues lock in order to prove that the specified page doesn't
	 * exist.
	 */
	VM_OBJECT_ASSERT_WLOCKED(object);
	if (__predict_true(vm_object_cache_is_empty(object)))
		return (FALSE);
	mtx_lock(&vm_page_queue_free_mtx);
	m = vm_page_cache_lookup(object, pindex);
	mtx_unlock(&vm_page_queue_free_mtx);
	return (m != NULL);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a page that is associated with the specified
 *	object and offset pair.  By default, this page is exclusive busied.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_COUNT(number)	the number of additional pages that the caller
 *				intends to allocate
 *	VM_ALLOC_IFCACHED	return page only if it is cached
 *	VM_ALLOC_IFNOTCACHED	return NULL, do not reactivate if the page
 *				is cached
 *	VM_ALLOC_NOBUSY		do not exclusive busy the page
 *	VM_ALLOC_NODUMP		do not include the page in a kernel core dump
 *	VM_ALLOC_NOOBJ		page is not associated with an object and
 *				should not be exclusive busy 
 *	VM_ALLOC_SBUSY		shared busy the allocated page
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_ZERO		prefer a zeroed page
 *
 *	This routine may not sleep.
 */
vm_page_t
vm_page_alloc(vm_object_t object, vm_pindex_t pindex, int req)
{
	struct vnode *vp = NULL;
	vm_object_t m_object;
	vm_page_t m, mpred;
	int flags, req_class;

	mpred = 0;	/* XXX: pacify gcc */
	KASSERT((object != NULL) == ((req & VM_ALLOC_NOOBJ) == 0) &&
	    (object != NULL || (req & VM_ALLOC_SBUSY) == 0) &&
	    ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)) !=
	    (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)),
	    ("vm_page_alloc: inconsistent object(%p)/req(%x)", (void *)object,
	    req));
	if (object != NULL)
		VM_OBJECT_ASSERT_WLOCKED(object);

	req_class = req & VM_ALLOC_CLASS_MASK;

	/*
	 * The page daemon is allowed to dig deeper into the free page list.
	 */
	if (curproc == pageproc && req_class != VM_ALLOC_INTERRUPT)
		req_class = VM_ALLOC_SYSTEM;

	if (object != NULL) {
		mpred = vm_radix_lookup_le(&object->rtree, pindex);
		KASSERT(mpred == NULL || mpred->pindex != pindex,
		   ("vm_page_alloc: pindex already allocated"));
	}

	/*
	 * The page allocation request can came from consumers which already
	 * hold the free page queue mutex, like vm_page_insert() in
	 * vm_page_cache().
	 */
	mtx_lock_flags(&vm_page_queue_free_mtx, MTX_RECURSE);
	if (cnt.v_free_count + cnt.v_cache_count > cnt.v_free_reserved ||
	    (req_class == VM_ALLOC_SYSTEM &&
	    cnt.v_free_count + cnt.v_cache_count > cnt.v_interrupt_free_min) ||
	    (req_class == VM_ALLOC_INTERRUPT &&
	    cnt.v_free_count + cnt.v_cache_count > 0)) {
		/*
		 * Allocate from the free queue if the number of free pages
		 * exceeds the minimum for the request class.
		 */
		if (object != NULL &&
		    (m = vm_page_cache_lookup(object, pindex)) != NULL) {
			if ((req & VM_ALLOC_IFNOTCACHED) != 0) {
				mtx_unlock(&vm_page_queue_free_mtx);
				return (NULL);
			}
			if (vm_phys_unfree_page(m))
				vm_phys_set_pool(VM_FREEPOOL_DEFAULT, m, 0);
#if VM_NRESERVLEVEL > 0
			else if (!vm_reserv_reactivate_page(m))
#else
			else
#endif
				panic("vm_page_alloc: cache page %p is missing"
				    " from the free queue", m);
		} else if ((req & VM_ALLOC_IFCACHED) != 0) {
			mtx_unlock(&vm_page_queue_free_mtx);
			return (NULL);
#if VM_NRESERVLEVEL > 0
		} else if (object == NULL || (object->flags & (OBJ_COLORED |
		    OBJ_FICTITIOUS)) != OBJ_COLORED || (m =
		    vm_reserv_alloc_page(object, pindex, mpred)) == NULL) {
#else
		} else {
#endif
			m = vm_phys_alloc_pages(object != NULL ?
			    VM_FREEPOOL_DEFAULT : VM_FREEPOOL_DIRECT, 0);
#if VM_NRESERVLEVEL > 0
			if (m == NULL && vm_reserv_reclaim_inactive()) {
				m = vm_phys_alloc_pages(object != NULL ?
				    VM_FREEPOOL_DEFAULT : VM_FREEPOOL_DIRECT,
				    0);
			}
#endif
		}
	} else {
		/*
		 * Not allocatable, give up.
		 */
		mtx_unlock(&vm_page_queue_free_mtx);
		atomic_add_int(&vm_pageout_deficit,
		    max((u_int)req >> VM_ALLOC_COUNT_SHIFT, 1));
		pagedaemon_wakeup();
		return (NULL);
	}

	/*
	 *  At this point we had better have found a good page.
	 */
	KASSERT(m != NULL, ("vm_page_alloc: missing page"));
	KASSERT(m->queue == PQ_NONE,
	    ("vm_page_alloc: page %p has unexpected queue %d", m, m->queue));
	KASSERT(m->wire_count == 0, ("vm_page_alloc: page %p is wired", m));
	KASSERT(m->hold_count == 0, ("vm_page_alloc: page %p is held", m));
	KASSERT(!vm_page_sbusied(m), 
	    ("vm_page_alloc: page %p is busy", m));
	KASSERT(m->dirty == 0, ("vm_page_alloc: page %p is dirty", m));
	KASSERT(pmap_page_get_memattr(m) == VM_MEMATTR_DEFAULT,
	    ("vm_page_alloc: page %p has unexpected memattr %d", m,
	    pmap_page_get_memattr(m)));
	if ((m->flags & PG_CACHED) != 0) {
		KASSERT((m->flags & PG_ZERO) == 0,
		    ("vm_page_alloc: cached page %p is PG_ZERO", m));
		KASSERT(m->valid != 0,
		    ("vm_page_alloc: cached page %p is invalid", m));
		if (m->object == object && m->pindex == pindex)
	  		cnt.v_reactivated++;
		else
			m->valid = 0;
		m_object = m->object;
		vm_page_cache_remove(m);
		if (m_object->type == OBJT_VNODE &&
		    vm_object_cache_is_empty(m_object))
			vp = m_object->handle;
	} else {
		KASSERT(VM_PAGE_IS_FREE(m),
		    ("vm_page_alloc: page %p is not free", m));
		KASSERT(m->valid == 0,
		    ("vm_page_alloc: free page %p is valid", m));
		vm_phys_freecnt_adj(m, -1);
	}

	/*
	 * Only the PG_ZERO flag is inherited.  The PG_CACHED or PG_FREE flag
	 * must be cleared before the free page queues lock is released.
	 */
	flags = 0;
	if (m->flags & PG_ZERO) {
		vm_page_zero_count--;
		if (req & VM_ALLOC_ZERO)
			flags = PG_ZERO;
	}
	if (req & VM_ALLOC_NODUMP)
		flags |= PG_NODUMP;
	m->flags = flags;
	mtx_unlock(&vm_page_queue_free_mtx);
	m->aflags = 0;
	m->oflags = object == NULL || (object->flags & OBJ_UNMANAGED) != 0 ?
	    VPO_UNMANAGED : 0;
	m->busy_lock = VPB_UNBUSIED;
	if ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_NOOBJ | VM_ALLOC_SBUSY)) == 0)
		m->busy_lock = VPB_SINGLE_EXCLUSIVER;
	if ((req & VM_ALLOC_SBUSY) != 0)
		m->busy_lock = VPB_SHARERS_WORD(1);
	if (req & VM_ALLOC_WIRED) {
		/*
		 * The page lock is not required for wiring a page until that
		 * page is inserted into the object.
		 */
		atomic_add_int(&cnt.v_wire_count, 1);
		m->wire_count = 1;
	}
	m->act_count = 0;

	if (object != NULL) {
		if (vm_page_insert_after(m, object, pindex, mpred)) {
			/* See the comment below about hold count. */
			if (vp != NULL)
				vdrop(vp);
			pagedaemon_wakeup();
			if (req & VM_ALLOC_WIRED) {
				atomic_subtract_int(&cnt.v_wire_count, 1);
				m->wire_count = 0;
			}
			m->object = NULL;
			vm_page_free(m);
			return (NULL);
		}

		/* Ignore device objects; the pager sets "memattr" for them. */
		if (object->memattr != VM_MEMATTR_DEFAULT &&
		    (object->flags & OBJ_FICTITIOUS) == 0)
			pmap_page_set_memattr(m, object->memattr);
	} else
		m->pindex = pindex;

	/*
	 * The following call to vdrop() must come after the above call
	 * to vm_page_insert() in case both affect the same object and
	 * vnode.  Otherwise, the affected vnode's hold count could
	 * temporarily become zero.
	 */
	if (vp != NULL)
		vdrop(vp);

	/*
	 * Don't wakeup too often - wakeup the pageout daemon when
	 * we would be nearly out of memory.
	 */
	if (vm_paging_needed())
		pagedaemon_wakeup();

	return (m);
}

static void
vm_page_alloc_contig_vdrop(struct spglist *lst)
{

	while (!SLIST_EMPTY(lst)) {
		vdrop((struct vnode *)SLIST_FIRST(lst)-> plinks.s.pv);
		SLIST_REMOVE_HEAD(lst, plinks.s.ss);
	}
}

/*
 *	vm_page_alloc_contig:
 *
 *	Allocate a contiguous set of physical pages of the given size "npages"
 *	from the free lists.  All of the physical pages must be at or above
 *	the given physical address "low" and below the given physical address
 *	"high".  The given value "alignment" determines the alignment of the
 *	first physical page in the set.  If the given value "boundary" is
 *	non-zero, then the set of physical pages cannot cross any physical
 *	address boundary that is a multiple of that value.  Both "alignment"
 *	and "boundary" must be a power of two.
 *
 *	If the specified memory attribute, "memattr", is VM_MEMATTR_DEFAULT,
 *	then the memory attribute setting for the physical pages is configured
 *	to the object's memory attribute setting.  Otherwise, the memory
 *	attribute setting for the physical pages is configured to "memattr",
 *	overriding the object's memory attribute setting.  However, if the
 *	object's memory attribute setting is not VM_MEMATTR_DEFAULT, then the
 *	memory attribute setting for the physical pages cannot be configured
 *	to VM_MEMATTR_DEFAULT.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_NOBUSY		do not exclusive busy the page
 *	VM_ALLOC_NOOBJ		page is not associated with an object and
 *				should not be exclusive busy 
 *	VM_ALLOC_SBUSY		shared busy the allocated page
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_ZERO		prefer a zeroed page
 *
 *	This routine may not sleep.
 */
vm_page_t
vm_page_alloc_contig(vm_object_t object, vm_pindex_t pindex, int req,
    u_long npages, vm_paddr_t low, vm_paddr_t high, u_long alignment,
    vm_paddr_t boundary, vm_memattr_t memattr)
{
	struct vnode *drop;
	struct spglist deferred_vdrop_list;
	vm_page_t m, m_tmp, m_ret;
	u_int flags, oflags;
	int req_class;

	KASSERT((object != NULL) == ((req & VM_ALLOC_NOOBJ) == 0) &&
	    (object != NULL || (req & VM_ALLOC_SBUSY) == 0) &&
	    ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)) !=
	    (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)),
	    ("vm_page_alloc: inconsistent object(%p)/req(%x)", (void *)object,
	    req));
	if (object != NULL) {
		VM_OBJECT_ASSERT_WLOCKED(object);
		KASSERT(object->type == OBJT_PHYS,
		    ("vm_page_alloc_contig: object %p isn't OBJT_PHYS",
		    object));
	}
	KASSERT(npages > 0, ("vm_page_alloc_contig: npages is zero"));
	req_class = req & VM_ALLOC_CLASS_MASK;

	/*
	 * The page daemon is allowed to dig deeper into the free page list.
	 */
	if (curproc == pageproc && req_class != VM_ALLOC_INTERRUPT)
		req_class = VM_ALLOC_SYSTEM;

	SLIST_INIT(&deferred_vdrop_list);
	mtx_lock(&vm_page_queue_free_mtx);
	if (cnt.v_free_count + cnt.v_cache_count >= npages +
	    cnt.v_free_reserved || (req_class == VM_ALLOC_SYSTEM &&
	    cnt.v_free_count + cnt.v_cache_count >= npages +
	    cnt.v_interrupt_free_min) || (req_class == VM_ALLOC_INTERRUPT &&
	    cnt.v_free_count + cnt.v_cache_count >= npages)) {
#if VM_NRESERVLEVEL > 0
retry:
		if (object == NULL || (object->flags & OBJ_COLORED) == 0 ||
		    (m_ret = vm_reserv_alloc_contig(object, pindex, npages,
		    low, high, alignment, boundary)) == NULL)
#endif
			m_ret = vm_phys_alloc_contig(npages, low, high,
			    alignment, boundary);
	} else {
		mtx_unlock(&vm_page_queue_free_mtx);
		atomic_add_int(&vm_pageout_deficit, npages);
		pagedaemon_wakeup();
		return (NULL);
	}
	if (m_ret != NULL)
		for (m = m_ret; m < &m_ret[npages]; m++) {
			drop = vm_page_alloc_init(m);
			if (drop != NULL) {
				/*
				 * Enqueue the vnode for deferred vdrop().
				 */
				m->plinks.s.pv = drop;
				SLIST_INSERT_HEAD(&deferred_vdrop_list, m,
				    plinks.s.ss);
			}
		}
	else {
#if VM_NRESERVLEVEL > 0
		if (vm_reserv_reclaim_contig(npages, low, high, alignment,
		    boundary))
			goto retry;
#endif
	}
	mtx_unlock(&vm_page_queue_free_mtx);
	if (m_ret == NULL)
		return (NULL);

	/*
	 * Initialize the pages.  Only the PG_ZERO flag is inherited.
	 */
	flags = 0;
	if ((req & VM_ALLOC_ZERO) != 0)
		flags = PG_ZERO;
	if ((req & VM_ALLOC_NODUMP) != 0)
		flags |= PG_NODUMP;
	if ((req & VM_ALLOC_WIRED) != 0)
		atomic_add_int(&cnt.v_wire_count, npages);
	oflags = VPO_UNMANAGED;
	if (object != NULL) {
		if (object->memattr != VM_MEMATTR_DEFAULT &&
		    memattr == VM_MEMATTR_DEFAULT)
			memattr = object->memattr;
	}
	for (m = m_ret; m < &m_ret[npages]; m++) {
		m->aflags = 0;
		m->flags = (m->flags | PG_NODUMP) & flags;
		m->busy_lock = VPB_UNBUSIED;
		if (object != NULL) {
			if ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)) == 0)
				m->busy_lock = VPB_SINGLE_EXCLUSIVER;
			if ((req & VM_ALLOC_SBUSY) != 0)
				m->busy_lock = VPB_SHARERS_WORD(1);
		}
		if ((req & VM_ALLOC_WIRED) != 0)
			m->wire_count = 1;
		/* Unmanaged pages don't use "act_count". */
		m->oflags = oflags;
		if (object != NULL) {
			if (vm_page_insert(m, object, pindex)) {
				vm_page_alloc_contig_vdrop(
				    &deferred_vdrop_list);
				if (vm_paging_needed())
					pagedaemon_wakeup();
				if ((req & VM_ALLOC_WIRED) != 0)
					atomic_subtract_int(&cnt.v_wire_count,
					    npages);
				for (m_tmp = m, m = m_ret;
				    m < &m_ret[npages]; m++) {
					if ((req & VM_ALLOC_WIRED) != 0)
						m->wire_count = 0;
					if (m >= m_tmp)
						m->object = NULL;
					vm_page_free(m);
				}
				return (NULL);
			}
		} else
			m->pindex = pindex;
		if (memattr != VM_MEMATTR_DEFAULT)
			pmap_page_set_memattr(m, memattr);
		pindex++;
	}
	vm_page_alloc_contig_vdrop(&deferred_vdrop_list);
	if (vm_paging_needed())
		pagedaemon_wakeup();
	return (m_ret);
}

/*
 * Initialize a page that has been freshly dequeued from a freelist.
 * The caller has to drop the vnode returned, if it is not NULL.
 *
 * This function may only be used to initialize unmanaged pages.
 *
 * To be called with vm_page_queue_free_mtx held.
 */
static struct vnode *
vm_page_alloc_init(vm_page_t m)
{
	struct vnode *drop;
	vm_object_t m_object;

	KASSERT(m->queue == PQ_NONE,
	    ("vm_page_alloc_init: page %p has unexpected queue %d",
	    m, m->queue));
	KASSERT(m->wire_count == 0,
	    ("vm_page_alloc_init: page %p is wired", m));
	KASSERT(m->hold_count == 0,
	    ("vm_page_alloc_init: page %p is held", m));
	KASSERT(!vm_page_sbusied(m),
	    ("vm_page_alloc_init: page %p is busy", m));
	KASSERT(m->dirty == 0,
	    ("vm_page_alloc_init: page %p is dirty", m));
	KASSERT(pmap_page_get_memattr(m) == VM_MEMATTR_DEFAULT,
	    ("vm_page_alloc_init: page %p has unexpected memattr %d",
	    m, pmap_page_get_memattr(m)));
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	drop = NULL;
	if ((m->flags & PG_CACHED) != 0) {
		KASSERT((m->flags & PG_ZERO) == 0,
		    ("vm_page_alloc_init: cached page %p is PG_ZERO", m));
		m->valid = 0;
		m_object = m->object;
		vm_page_cache_remove(m);
		if (m_object->type == OBJT_VNODE &&
		    vm_object_cache_is_empty(m_object))
			drop = m_object->handle;
	} else {
		KASSERT(VM_PAGE_IS_FREE(m),
		    ("vm_page_alloc_init: page %p is not free", m));
		KASSERT(m->valid == 0,
		    ("vm_page_alloc_init: free page %p is valid", m));
		vm_phys_freecnt_adj(m, -1);
		if ((m->flags & PG_ZERO) != 0)
			vm_page_zero_count--;
	}
	/* Don't clear the PG_ZERO flag; we'll need it later. */
	m->flags &= PG_ZERO;
	return (drop);
}

/*
 * 	vm_page_alloc_freelist:
 *
 *	Allocate a physical page from the specified free page list.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_COUNT(number)	the number of additional pages that the caller
 *				intends to allocate
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_ZERO		prefer a zeroed page
 *
 *	This routine may not sleep.
 */
vm_page_t
vm_page_alloc_freelist(int flind, int req)
{
	struct vnode *drop;
	vm_page_t m;
	u_int flags;
	int req_class;

	req_class = req & VM_ALLOC_CLASS_MASK;

	/*
	 * The page daemon is allowed to dig deeper into the free page list.
	 */
	if (curproc == pageproc && req_class != VM_ALLOC_INTERRUPT)
		req_class = VM_ALLOC_SYSTEM;

	/*
	 * Do not allocate reserved pages unless the req has asked for it.
	 */
	mtx_lock_flags(&vm_page_queue_free_mtx, MTX_RECURSE);
	if (cnt.v_free_count + cnt.v_cache_count > cnt.v_free_reserved ||
	    (req_class == VM_ALLOC_SYSTEM &&
	    cnt.v_free_count + cnt.v_cache_count > cnt.v_interrupt_free_min) ||
	    (req_class == VM_ALLOC_INTERRUPT &&
	    cnt.v_free_count + cnt.v_cache_count > 0))
		m = vm_phys_alloc_freelist_pages(flind, VM_FREEPOOL_DIRECT, 0);
	else {
		mtx_unlock(&vm_page_queue_free_mtx);
		atomic_add_int(&vm_pageout_deficit,
		    max((u_int)req >> VM_ALLOC_COUNT_SHIFT, 1));
		pagedaemon_wakeup();
		return (NULL);
	}
	if (m == NULL) {
		mtx_unlock(&vm_page_queue_free_mtx);
		return (NULL);
	}
	drop = vm_page_alloc_init(m);
	mtx_unlock(&vm_page_queue_free_mtx);

	/*
	 * Initialize the page.  Only the PG_ZERO flag is inherited.
	 */
	m->aflags = 0;
	flags = 0;
	if ((req & VM_ALLOC_ZERO) != 0)
		flags = PG_ZERO;
	m->flags &= flags;
	if ((req & VM_ALLOC_WIRED) != 0) {
		/*
		 * The page lock is not required for wiring a page that does
		 * not belong to an object.
		 */
		atomic_add_int(&cnt.v_wire_count, 1);
		m->wire_count = 1;
	}
	/* Unmanaged pages don't use "act_count". */
	m->oflags = VPO_UNMANAGED;
	if (drop != NULL)
		vdrop(drop);
	if (vm_paging_needed())
		pagedaemon_wakeup();
	return (m);
}

/*
 *	vm_wait:	(also see VM_WAIT macro)
 *
 *	Sleep until free pages are available for allocation.
 *	- Called in various places before memory allocations.
 */
void
vm_wait(void)
{

	mtx_lock(&vm_page_queue_free_mtx);
	if (curproc == pageproc) {
		vm_pageout_pages_needed = 1;
		msleep(&vm_pageout_pages_needed, &vm_page_queue_free_mtx,
		    PDROP | PSWP, "VMWait", 0);
	} else {
		if (!vm_pages_needed) {
			vm_pages_needed = 1;
			wakeup(&vm_pages_needed);
		}
		msleep(&cnt.v_free_count, &vm_page_queue_free_mtx, PDROP | PVM,
		    "vmwait", 0);
	}
}

/*
 *	vm_waitpfault:	(also see VM_WAITPFAULT macro)
 *
 *	Sleep until free pages are available for allocation.
 *	- Called only in vm_fault so that processes page faulting
 *	  can be easily tracked.
 *	- Sleeps at a lower priority than vm_wait() so that vm_wait()ing
 *	  processes will be able to grab memory first.  Do not change
 *	  this balance without careful testing first.
 */
void
vm_waitpfault(void)
{

	mtx_lock(&vm_page_queue_free_mtx);
	if (!vm_pages_needed) {
		vm_pages_needed = 1;
		wakeup(&vm_pages_needed);
	}
	msleep(&cnt.v_free_count, &vm_page_queue_free_mtx, PDROP | PUSER,
	    "pfault", 0);
}

struct vm_pagequeue *
vm_page_pagequeue(vm_page_t m)
{

	return (&vm_phys_domain(m)->vmd_pagequeues[m->queue]);
}

/*
 *	vm_page_dequeue:
 *
 *	Remove the given page from its current page queue.
 *
 *	The page must be locked.
 */
void
vm_page_dequeue(vm_page_t m)
{
	struct vm_pagequeue *pq;

	vm_page_lock_assert(m, MA_OWNED);
	KASSERT(m->queue != PQ_NONE,
	    ("vm_page_dequeue: page %p is not queued", m));
	pq = vm_page_pagequeue(m);
	vm_pagequeue_lock(pq);
	m->queue = PQ_NONE;
	TAILQ_REMOVE(&pq->pq_pl, m, plinks.q);
	vm_pagequeue_cnt_dec(pq);
	vm_pagequeue_unlock(pq);
}

/*
 *	vm_page_dequeue_locked:
 *
 *	Remove the given page from its current page queue.
 *
 *	The page and page queue must be locked.
 */
void
vm_page_dequeue_locked(vm_page_t m)
{
	struct vm_pagequeue *pq;

	vm_page_lock_assert(m, MA_OWNED);
	pq = vm_page_pagequeue(m);
	vm_pagequeue_assert_locked(pq);
	m->queue = PQ_NONE;
	TAILQ_REMOVE(&pq->pq_pl, m, plinks.q);
	vm_pagequeue_cnt_dec(pq);
}

/*
 *	vm_page_enqueue:
 *
 *	Add the given page to the specified page queue.
 *
 *	The page must be locked.
 */
static void
vm_page_enqueue(int queue, vm_page_t m)
{
	struct vm_pagequeue *pq;

	vm_page_lock_assert(m, MA_OWNED);
	pq = &vm_phys_domain(m)->vmd_pagequeues[queue];
	vm_pagequeue_lock(pq);
	m->queue = queue;
	TAILQ_INSERT_TAIL(&pq->pq_pl, m, plinks.q);
	vm_pagequeue_cnt_inc(pq);
	vm_pagequeue_unlock(pq);
}

/*
 *	vm_page_requeue:
 *
 *	Move the given page to the tail of its current page queue.
 *
 *	The page must be locked.
 */
void
vm_page_requeue(vm_page_t m)
{
	struct vm_pagequeue *pq;

	vm_page_lock_assert(m, MA_OWNED);
	KASSERT(m->queue != PQ_NONE,
	    ("vm_page_requeue: page %p is not queued", m));
	pq = vm_page_pagequeue(m);
	vm_pagequeue_lock(pq);
	TAILQ_REMOVE(&pq->pq_pl, m, plinks.q);
	TAILQ_INSERT_TAIL(&pq->pq_pl, m, plinks.q);
	vm_pagequeue_unlock(pq);
}

/*
 *	vm_page_requeue_locked:
 *
 *	Move the given page to the tail of its current page queue.
 *
 *	The page queue must be locked.
 */
void
vm_page_requeue_locked(vm_page_t m)
{
	struct vm_pagequeue *pq;

	KASSERT(m->queue != PQ_NONE,
	    ("vm_page_requeue_locked: page %p is not queued", m));
	pq = vm_page_pagequeue(m);
	vm_pagequeue_assert_locked(pq);
	TAILQ_REMOVE(&pq->pq_pl, m, plinks.q);
	TAILQ_INSERT_TAIL(&pq->pq_pl, m, plinks.q);
}

/*
 *	vm_page_activate:
 *
 *	Put the specified page on the active list (if appropriate).
 *	Ensure that act_count is at least ACT_INIT but do not otherwise
 *	mess with it.
 *
 *	The page must be locked.
 */
void
vm_page_activate(vm_page_t m)
{
	int queue;

	vm_page_lock_assert(m, MA_OWNED);
	if ((queue = m->queue) != PQ_ACTIVE) {
		if (m->wire_count == 0 && (m->oflags & VPO_UNMANAGED) == 0) {
			if (m->act_count < ACT_INIT)
				m->act_count = ACT_INIT;
			if (queue != PQ_NONE)
				vm_page_dequeue(m);
			vm_page_enqueue(PQ_ACTIVE, m);
		} else
			KASSERT(queue == PQ_NONE,
			    ("vm_page_activate: wired page %p is queued", m));
	} else {
		if (m->act_count < ACT_INIT)
			m->act_count = ACT_INIT;
	}
}

/*
 *	vm_page_free_wakeup:
 *
 *	Helper routine for vm_page_free_toq() and vm_page_cache().  This
 *	routine is called when a page has been added to the cache or free
 *	queues.
 *
 *	The page queues must be locked.
 */
static inline void
vm_page_free_wakeup(void)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	/*
	 * if pageout daemon needs pages, then tell it that there are
	 * some free.
	 */
	if (vm_pageout_pages_needed &&
	    cnt.v_cache_count + cnt.v_free_count >= cnt.v_pageout_free_min) {
		wakeup(&vm_pageout_pages_needed);
		vm_pageout_pages_needed = 0;
	}
	/*
	 * wakeup processes that are waiting on memory if we hit a
	 * high water mark. And wakeup scheduler process if we have
	 * lots of memory. this process will swapin processes.
	 */
	if (vm_pages_needed && !vm_page_count_min()) {
		vm_pages_needed = 0;
		wakeup(&cnt.v_free_count);
	}
}

/*
 *	Turn a cached page into a free page, by changing its attributes.
 *	Keep the statistics up-to-date.
 *
 *	The free page queue must be locked.
 */
static void
vm_page_cache_turn_free(vm_page_t m)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);

	m->object = NULL;
	m->valid = 0;
	/* Clear PG_CACHED and set PG_FREE. */
	m->flags ^= PG_CACHED | PG_FREE;
	KASSERT((m->flags & (PG_CACHED | PG_FREE)) == PG_FREE,
	    ("vm_page_cache_free: page %p has inconsistent flags", m));
	cnt.v_cache_count--;
	vm_phys_freecnt_adj(m, 1);
}

/*
 *	vm_page_free_toq:
 *
 *	Returns the given page to the free list,
 *	disassociating it with any VM object.
 *
 *	The object must be locked.  The page must be locked if it is managed.
 */
void
vm_page_free_toq(vm_page_t m)
{

	if ((m->oflags & VPO_UNMANAGED) == 0) {
		vm_page_lock_assert(m, MA_OWNED);
		KASSERT(!pmap_page_is_mapped(m),
		    ("vm_page_free_toq: freeing mapped page %p", m));
	} else
		KASSERT(m->queue == PQ_NONE,
		    ("vm_page_free_toq: unmanaged page %p is queued", m));
	PCPU_INC(cnt.v_tfree);

	if (VM_PAGE_IS_FREE(m))
		panic("vm_page_free: freeing free page %p", m);
	else if (vm_page_sbusied(m))
		panic("vm_page_free: freeing busy page %p", m);

	/*
	 * Unqueue, then remove page.  Note that we cannot destroy
	 * the page here because we do not want to call the pager's
	 * callback routine until after we've put the page on the
	 * appropriate free queue.
	 */
	vm_page_remque(m);
	vm_page_remove(m);

	/*
	 * If fictitious remove object association and
	 * return, otherwise delay object association removal.
	 */
	if ((m->flags & PG_FICTITIOUS) != 0) {
		return;
	}

	m->valid = 0;
	vm_page_undirty(m);

	if (m->wire_count != 0)
		panic("vm_page_free: freeing wired page %p", m);
	if (m->hold_count != 0) {
		m->flags &= ~PG_ZERO;
		KASSERT((m->flags & PG_UNHOLDFREE) == 0,
		    ("vm_page_free: freeing PG_UNHOLDFREE page %p", m));
		m->flags |= PG_UNHOLDFREE;
	} else {
		/*
		 * Restore the default memory attribute to the page.
		 */
		if (pmap_page_get_memattr(m) != VM_MEMATTR_DEFAULT)
			pmap_page_set_memattr(m, VM_MEMATTR_DEFAULT);

		/*
		 * Insert the page into the physical memory allocator's
		 * cache/free page queues.
		 */
		mtx_lock(&vm_page_queue_free_mtx);
		m->flags |= PG_FREE;
		vm_phys_freecnt_adj(m, 1);
#if VM_NRESERVLEVEL > 0
		if (!vm_reserv_free_page(m))
#else
		if (TRUE)
#endif
			vm_phys_free_pages(m, 0);
		if ((m->flags & PG_ZERO) != 0)
			++vm_page_zero_count;
		else
			vm_page_zero_idle_wakeup();
		vm_page_free_wakeup();
		mtx_unlock(&vm_page_queue_free_mtx);
	}
}

/*
 *	vm_page_wire:
 *
 *	Mark this page as wired down by yet
 *	another map, removing it from paging queues
 *	as necessary.
 *
 *	If the page is fictitious, then its wire count must remain one.
 *
 *	The page must be locked.
 */
void
vm_page_wire(vm_page_t m)
{

	/*
	 * Only bump the wire statistics if the page is not already wired,
	 * and only unqueue the page if it is on some queue (if it is unmanaged
	 * it is already off the queues).
	 */
	vm_page_lock_assert(m, MA_OWNED);
	if ((m->flags & PG_FICTITIOUS) != 0) {
		KASSERT(m->wire_count == 1,
		    ("vm_page_wire: fictitious page %p's wire count isn't one",
		    m));
		return;
	}
	if (m->wire_count == 0) {
		KASSERT((m->oflags & VPO_UNMANAGED) == 0 ||
		    m->queue == PQ_NONE,
		    ("vm_page_wire: unmanaged page %p is queued", m));
		vm_page_remque(m);
		atomic_add_int(&cnt.v_wire_count, 1);
	}
	m->wire_count++;
	KASSERT(m->wire_count != 0, ("vm_page_wire: wire_count overflow m=%p", m));
}

/*
 * vm_page_unwire:
 *
 * Release one wiring of the specified page, potentially enabling it to be
 * paged again.  If paging is enabled, then the value of the parameter
 * "activate" determines to which queue the page is added.  If "activate" is
 * non-zero, then the page is added to the active queue.  Otherwise, it is
 * added to the inactive queue.
 *
 * However, unless the page belongs to an object, it is not enqueued because
 * it cannot be paged out.
 *
 * If a page is fictitious, then its wire count must always be one.
 *
 * A managed page must be locked.
 */
void
vm_page_unwire(vm_page_t m, int activate)
{

	if ((m->oflags & VPO_UNMANAGED) == 0)
		vm_page_lock_assert(m, MA_OWNED);
	if ((m->flags & PG_FICTITIOUS) != 0) {
		KASSERT(m->wire_count == 1,
	    ("vm_page_unwire: fictitious page %p's wire count isn't one", m));
		return;
	}
	if (m->wire_count > 0) {
		m->wire_count--;
		if (m->wire_count == 0) {
			atomic_subtract_int(&cnt.v_wire_count, 1);
			if ((m->oflags & VPO_UNMANAGED) != 0 ||
			    m->object == NULL)
				return;
			if (!activate)
				m->flags &= ~PG_WINATCFLS;
			vm_page_enqueue(activate ? PQ_ACTIVE : PQ_INACTIVE, m);
		}
	} else
		panic("vm_page_unwire: page %p's wire count is zero", m);
}

/*
 * Move the specified page to the inactive queue.
 *
 * Many pages placed on the inactive queue should actually go
 * into the cache, but it is difficult to figure out which.  What
 * we do instead, if the inactive target is well met, is to put
 * clean pages at the head of the inactive queue instead of the tail.
 * This will cause them to be moved to the cache more quickly and
 * if not actively re-referenced, reclaimed more quickly.  If we just
 * stick these pages at the end of the inactive queue, heavy filesystem
 * meta-data accesses can cause an unnecessary paging load on memory bound 
 * processes.  This optimization causes one-time-use metadata to be
 * reused more quickly.
 *
 * Normally athead is 0 resulting in LRU operation.  athead is set
 * to 1 if we want this page to be 'as if it were placed in the cache',
 * except without unmapping it from the process address space.
 *
 * The page must be locked.
 */
static inline void
_vm_page_deactivate(vm_page_t m, int athead)
{
	struct vm_pagequeue *pq;
	int queue;

	vm_page_lock_assert(m, MA_OWNED);

	/*
	 * Ignore if already inactive.
	 */
	if ((queue = m->queue) == PQ_INACTIVE)
		return;
	if (m->wire_count == 0 && (m->oflags & VPO_UNMANAGED) == 0) {
		if (queue != PQ_NONE)
			vm_page_dequeue(m);
		m->flags &= ~PG_WINATCFLS;
		pq = &vm_phys_domain(m)->vmd_pagequeues[PQ_INACTIVE];
		vm_pagequeue_lock(pq);
		m->queue = PQ_INACTIVE;
		if (athead)
			TAILQ_INSERT_HEAD(&pq->pq_pl, m, plinks.q);
		else
			TAILQ_INSERT_TAIL(&pq->pq_pl, m, plinks.q);
		vm_pagequeue_cnt_inc(pq);
		vm_pagequeue_unlock(pq);
	}
}

/*
 * Move the specified page to the inactive queue.
 *
 * The page must be locked.
 */
void
vm_page_deactivate(vm_page_t m)
{

	_vm_page_deactivate(m, 0);
}

/*
 * vm_page_try_to_cache:
 *
 * Returns 0 on failure, 1 on success
 */
int
vm_page_try_to_cache(vm_page_t m)
{

	vm_page_lock_assert(m, MA_OWNED);
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (m->dirty || m->hold_count || m->wire_count ||
	    (m->oflags & VPO_UNMANAGED) != 0 || vm_page_busied(m))
		return (0);
	pmap_remove_all(m);
	if (m->dirty)
		return (0);
	vm_page_cache(m);
	return (1);
}

/*
 * vm_page_try_to_free()
 *
 *	Attempt to free the page.  If we cannot free it, we do nothing.
 *	1 is returned on success, 0 on failure.
 */
int
vm_page_try_to_free(vm_page_t m)
{

	vm_page_lock_assert(m, MA_OWNED);
	if (m->object != NULL)
		VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (m->dirty || m->hold_count || m->wire_count ||
	    (m->oflags & VPO_UNMANAGED) != 0 || vm_page_busied(m))
		return (0);
	pmap_remove_all(m);
	if (m->dirty)
		return (0);
	vm_page_free(m);
	return (1);
}

/*
 * vm_page_cache
 *
 * Put the specified page onto the page cache queue (if appropriate).
 *
 * The object and page must be locked.
 */
void
vm_page_cache(vm_page_t m)
{
	vm_object_t object;
	boolean_t cache_was_empty;

	vm_page_lock_assert(m, MA_OWNED);
	object = m->object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	if (vm_page_busied(m) || (m->oflags & VPO_UNMANAGED) ||
	    m->hold_count || m->wire_count)
		panic("vm_page_cache: attempting to cache busy page");
	KASSERT(!pmap_page_is_mapped(m),
	    ("vm_page_cache: page %p is mapped", m));
	KASSERT(m->dirty == 0, ("vm_page_cache: page %p is dirty", m));
	if (m->valid == 0 || object->type == OBJT_DEFAULT ||
	    (object->type == OBJT_SWAP &&
	    !vm_pager_has_page(object, m->pindex, NULL, NULL))) {
		/*
		 * Hypothesis: A cache-elgible page belonging to a
		 * default object or swap object but without a backing
		 * store must be zero filled.
		 */
		vm_page_free(m);
		return;
	}
	KASSERT((m->flags & PG_CACHED) == 0,
	    ("vm_page_cache: page %p is already cached", m));

	/*
	 * Remove the page from the paging queues.
	 */
	vm_page_remque(m);

	/*
	 * Remove the page from the object's collection of resident
	 * pages. 
	 */
	vm_radix_remove(&object->rtree, m->pindex);
	TAILQ_REMOVE(&object->memq, m, listq);
	object->resident_page_count--;

	/*
	 * Restore the default memory attribute to the page.
	 */
	if (pmap_page_get_memattr(m) != VM_MEMATTR_DEFAULT)
		pmap_page_set_memattr(m, VM_MEMATTR_DEFAULT);

	/*
	 * Insert the page into the object's collection of cached pages
	 * and the physical memory allocator's cache/free page queues.
	 */
	m->flags &= ~PG_ZERO;
	mtx_lock(&vm_page_queue_free_mtx);
	cache_was_empty = vm_radix_is_empty(&object->cache);
	if (vm_radix_insert(&object->cache, m)) {
		mtx_unlock(&vm_page_queue_free_mtx);
		if (object->resident_page_count == 0)
			vdrop(object->handle);
		m->object = NULL;
		vm_page_free(m);
		return;
	}

	/*
	 * The above call to vm_radix_insert() could reclaim the one pre-
	 * existing cached page from this object, resulting in a call to
	 * vdrop().
	 */
	if (!cache_was_empty)
		cache_was_empty = vm_radix_is_singleton(&object->cache);

	m->flags |= PG_CACHED;
	cnt.v_cache_count++;
	PCPU_INC(cnt.v_tcached);
#if VM_NRESERVLEVEL > 0
	if (!vm_reserv_free_page(m)) {
#else
	if (TRUE) {
#endif
		vm_phys_set_pool(VM_FREEPOOL_CACHE, m, 0);
		vm_phys_free_pages(m, 0);
	}
	vm_page_free_wakeup();
	mtx_unlock(&vm_page_queue_free_mtx);

	/*
	 * Increment the vnode's hold count if this is the object's only
	 * cached page.  Decrement the vnode's hold count if this was
	 * the object's only resident page.
	 */
	if (object->type == OBJT_VNODE) {
		if (cache_was_empty && object->resident_page_count != 0)
			vhold(object->handle);
		else if (!cache_was_empty && object->resident_page_count == 0)
			vdrop(object->handle);
	}
}

/*
 * vm_page_advise
 *
 *	Cache, deactivate, or do nothing as appropriate.  This routine
 *	is used by madvise().
 *
 *	Generally speaking we want to move the page into the cache so
 *	it gets reused quickly.  However, this can result in a silly syndrome
 *	due to the page recycling too quickly.  Small objects will not be
 *	fully cached.  On the other hand, if we move the page to the inactive
 *	queue we wind up with a problem whereby very large objects 
 *	unnecessarily blow away our inactive and cache queues.
 *
 *	The solution is to move the pages based on a fixed weighting.  We
 *	either leave them alone, deactivate them, or move them to the cache,
 *	where moving them to the cache has the highest weighting.
 *	By forcing some pages into other queues we eventually force the
 *	system to balance the queues, potentially recovering other unrelated
 *	space from active.  The idea is to not force this to happen too
 *	often.
 *
 *	The object and page must be locked.
 */
void
vm_page_advise(vm_page_t m, int advice)
{
	int dnw, head;

	vm_page_assert_locked(m);
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (advice == MADV_FREE) {
		/*
		 * Mark the page clean.  This will allow the page to be freed
		 * up by the system.  However, such pages are often reused
		 * quickly by malloc() so we do not do anything that would
		 * cause a page fault if we can help it.
		 *
		 * Specifically, we do not try to actually free the page now
		 * nor do we try to put it in the cache (which would cause a
		 * page fault on reuse).
		 *
		 * But we do make the page is freeable as we can without
		 * actually taking the step of unmapping it.
		 */
		m->dirty = 0;
		m->act_count = 0;
	} else if (advice != MADV_DONTNEED)
		return;
	dnw = PCPU_GET(dnweight);
	PCPU_INC(dnweight);

	/*
	 * Occasionally leave the page alone.
	 */
	if ((dnw & 0x01F0) == 0 || m->queue == PQ_INACTIVE) {
		if (m->act_count >= ACT_INIT)
			--m->act_count;
		return;
	}

	/*
	 * Clear any references to the page.  Otherwise, the page daemon will
	 * immediately reactivate the page.
	 */
	vm_page_aflag_clear(m, PGA_REFERENCED);

	if (advice != MADV_FREE && m->dirty == 0 && pmap_is_modified(m))
		vm_page_dirty(m);

	if (m->dirty || (dnw & 0x0070) == 0) {
		/*
		 * Deactivate the page 3 times out of 32.
		 */
		head = 0;
	} else {
		/*
		 * Cache the page 28 times out of every 32.  Note that
		 * the page is deactivated instead of cached, but placed
		 * at the head of the queue instead of the tail.
		 */
		head = 1;
	}
	_vm_page_deactivate(m, head);
}

/*
 * Grab a page, waiting until we are waken up due to the page
 * changing state.  We keep on waiting, if the page continues
 * to be in the object.  If the page doesn't exist, first allocate it
 * and then conditionally zero it.
 *
 * This routine may sleep.
 *
 * The object must be locked on entry.  The lock will, however, be released
 * and reacquired if the routine sleeps.
 */
vm_page_t
vm_page_grab(vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;
	int sleep;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT((allocflags & VM_ALLOC_SBUSY) == 0 ||
	    (allocflags & VM_ALLOC_IGN_SBUSY) != 0,
	    ("vm_page_grab: VM_ALLOC_SBUSY/VM_ALLOC_IGN_SBUSY mismatch"));
retrylookup:
	if ((m = vm_page_lookup(object, pindex)) != NULL) {
		sleep = (allocflags & VM_ALLOC_IGN_SBUSY) != 0 ?
		    vm_page_xbusied(m) : vm_page_busied(m);
		if (sleep) {
			/*
			 * Reference the page before unlocking and
			 * sleeping so that the page daemon is less
			 * likely to reclaim it.
			 */
			vm_page_aflag_set(m, PGA_REFERENCED);
			vm_page_lock(m);
			VM_OBJECT_WUNLOCK(object);
			vm_page_busy_sleep(m, "pgrbwt");
			VM_OBJECT_WLOCK(object);
			goto retrylookup;
		} else {
			if ((allocflags & VM_ALLOC_WIRED) != 0) {
				vm_page_lock(m);
				vm_page_wire(m);
				vm_page_unlock(m);
			}
			if ((allocflags &
			    (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)) == 0)
				vm_page_xbusy(m);
			if ((allocflags & VM_ALLOC_SBUSY) != 0)
				vm_page_sbusy(m);
			return (m);
		}
	}
	m = vm_page_alloc(object, pindex, allocflags & ~VM_ALLOC_IGN_SBUSY);
	if (m == NULL) {
		VM_OBJECT_WUNLOCK(object);
		VM_WAIT;
		VM_OBJECT_WLOCK(object);
		goto retrylookup;
	} else if (m->valid != 0)
		return (m);
	if (allocflags & VM_ALLOC_ZERO && (m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	return (m);
}

/*
 * Mapping function for valid or dirty bits in a page.
 *
 * Inputs are required to range within a page.
 */
vm_page_bits_t
vm_page_bits(int base, int size)
{
	int first_bit;
	int last_bit;

	KASSERT(
	    base + size <= PAGE_SIZE,
	    ("vm_page_bits: illegal base/size %d/%d", base, size)
	);

	if (size == 0)		/* handle degenerate case */
		return (0);

	first_bit = base >> DEV_BSHIFT;
	last_bit = (base + size - 1) >> DEV_BSHIFT;

	return (((vm_page_bits_t)2 << last_bit) -
	    ((vm_page_bits_t)1 << first_bit));
}

/*
 *	vm_page_set_valid_range:
 *
 *	Sets portions of a page valid.  The arguments are expected
 *	to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 *	of any partial chunks touched by the range.  The invalid portion of
 *	such chunks will be zeroed.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_valid_range(vm_page_t m, int base, int size)
{
	int endoff, frag;

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (size == 0)	/* handle degenerate case */
		return;

	/*
	 * If the base is not DEV_BSIZE aligned and the valid
	 * bit is clear, we have to zero out a portion of the
	 * first block.
	 */
	if ((frag = base & ~(DEV_BSIZE - 1)) != base &&
	    (m->valid & (1 << (base >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, frag, base - frag);

	/*
	 * If the ending offset is not DEV_BSIZE aligned and the 
	 * valid bit is clear, we have to zero out a portion of
	 * the last block.
	 */
	endoff = base + size;
	if ((frag = endoff & ~(DEV_BSIZE - 1)) != endoff &&
	    (m->valid & (1 << (endoff >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, endoff,
		    DEV_BSIZE - (endoff & (DEV_BSIZE - 1)));

	/*
	 * Assert that no previously invalid block that is now being validated
	 * is already dirty. 
	 */
	KASSERT((~m->valid & vm_page_bits(base, size) & m->dirty) == 0,
	    ("vm_page_set_valid_range: page %p is dirty", m));

	/*
	 * Set valid bits inclusive of any overlap.
	 */
	m->valid |= vm_page_bits(base, size);
}

/*
 * Clear the given bits from the specified page's dirty field.
 */
static __inline void
vm_page_clear_dirty_mask(vm_page_t m, vm_page_bits_t pagebits)
{
	uintptr_t addr;
#if PAGE_SIZE < 16384
	int shift;
#endif

	/*
	 * If the object is locked and the page is neither exclusive busy nor
	 * write mapped, then the page's dirty field cannot possibly be
	 * set by a concurrent pmap operation.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && !pmap_page_is_write_mapped(m))
		m->dirty &= ~pagebits;
	else {
		/*
		 * The pmap layer can call vm_page_dirty() without
		 * holding a distinguished lock.  The combination of
		 * the object's lock and an atomic operation suffice
		 * to guarantee consistency of the page dirty field.
		 *
		 * For PAGE_SIZE == 32768 case, compiler already
		 * properly aligns the dirty field, so no forcible
		 * alignment is needed. Only require existence of
		 * atomic_clear_64 when page size is 32768.
		 */
		addr = (uintptr_t)&m->dirty;
#if PAGE_SIZE == 32768
		atomic_clear_64((uint64_t *)addr, pagebits);
#elif PAGE_SIZE == 16384
		atomic_clear_32((uint32_t *)addr, pagebits);
#else		/* PAGE_SIZE <= 8192 */
		/*
		 * Use a trick to perform a 32-bit atomic on the
		 * containing aligned word, to not depend on the existence
		 * of atomic_clear_{8, 16}.
		 */
		shift = addr & (sizeof(uint32_t) - 1);
#if BYTE_ORDER == BIG_ENDIAN
		shift = (sizeof(uint32_t) - sizeof(m->dirty) - shift) * NBBY;
#else
		shift *= NBBY;
#endif
		addr &= ~(sizeof(uint32_t) - 1);
		atomic_clear_32((uint32_t *)addr, pagebits << shift);
#endif		/* PAGE_SIZE */
	}
}

/*
 *	vm_page_set_validclean:
 *
 *	Sets portions of a page valid and clean.  The arguments are expected
 *	to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 *	of any partial chunks touched by the range.  The invalid portion of
 *	such chunks will be zero'd.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_validclean(vm_page_t m, int base, int size)
{
	vm_page_bits_t oldvalid, pagebits;
	int endoff, frag;

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (size == 0)	/* handle degenerate case */
		return;

	/*
	 * If the base is not DEV_BSIZE aligned and the valid
	 * bit is clear, we have to zero out a portion of the
	 * first block.
	 */
	if ((frag = base & ~(DEV_BSIZE - 1)) != base &&
	    (m->valid & ((vm_page_bits_t)1 << (base >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, frag, base - frag);

	/*
	 * If the ending offset is not DEV_BSIZE aligned and the 
	 * valid bit is clear, we have to zero out a portion of
	 * the last block.
	 */
	endoff = base + size;
	if ((frag = endoff & ~(DEV_BSIZE - 1)) != endoff &&
	    (m->valid & ((vm_page_bits_t)1 << (endoff >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, endoff,
		    DEV_BSIZE - (endoff & (DEV_BSIZE - 1)));

	/*
	 * Set valid, clear dirty bits.  If validating the entire
	 * page we can safely clear the pmap modify bit.  We also
	 * use this opportunity to clear the VPO_NOSYNC flag.  If a process
	 * takes a write fault on a MAP_NOSYNC memory area the flag will
	 * be set again.
	 *
	 * We set valid bits inclusive of any overlap, but we can only
	 * clear dirty bits for DEV_BSIZE chunks that are fully within
	 * the range.
	 */
	oldvalid = m->valid;
	pagebits = vm_page_bits(base, size);
	m->valid |= pagebits;
#if 0	/* NOT YET */
	if ((frag = base & (DEV_BSIZE - 1)) != 0) {
		frag = DEV_BSIZE - frag;
		base += frag;
		size -= frag;
		if (size < 0)
			size = 0;
	}
	pagebits = vm_page_bits(base, size & (DEV_BSIZE - 1));
#endif
	if (base == 0 && size == PAGE_SIZE) {
		/*
		 * The page can only be modified within the pmap if it is
		 * mapped, and it can only be mapped if it was previously
		 * fully valid.
		 */
		if (oldvalid == VM_PAGE_BITS_ALL)
			/*
			 * Perform the pmap_clear_modify() first.  Otherwise,
			 * a concurrent pmap operation, such as
			 * pmap_protect(), could clear a modification in the
			 * pmap and set the dirty field on the page before
			 * pmap_clear_modify() had begun and after the dirty
			 * field was cleared here.
			 */
			pmap_clear_modify(m);
		m->dirty = 0;
		m->oflags &= ~VPO_NOSYNC;
	} else if (oldvalid != VM_PAGE_BITS_ALL)
		m->dirty &= ~pagebits;
	else
		vm_page_clear_dirty_mask(m, pagebits);
}

void
vm_page_clear_dirty(vm_page_t m, int base, int size)
{

	vm_page_clear_dirty_mask(m, vm_page_bits(base, size));
}

/*
 *	vm_page_set_invalid:
 *
 *	Invalidates DEV_BSIZE'd chunks within a page.  Both the
 *	valid and dirty bits for the effected areas are cleared.
 */
void
vm_page_set_invalid(vm_page_t m, int base, int size)
{
	vm_page_bits_t bits;
	vm_object_t object;

	object = m->object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	if (object->type == OBJT_VNODE && base == 0 && IDX_TO_OFF(m->pindex) +
	    size >= object->un_pager.vnp.vnp_size)
		bits = VM_PAGE_BITS_ALL;
	else
		bits = vm_page_bits(base, size);
	if (m->valid == VM_PAGE_BITS_ALL && bits != 0)
		pmap_remove_all(m);
	KASSERT((bits == 0 && m->valid == VM_PAGE_BITS_ALL) ||
	    !pmap_page_is_mapped(m),
	    ("vm_page_set_invalid: page %p is mapped", m));
	m->valid &= ~bits;
	m->dirty &= ~bits;
}

/*
 * vm_page_zero_invalid()
 *
 *	The kernel assumes that the invalid portions of a page contain 
 *	garbage, but such pages can be mapped into memory by user code.
 *	When this occurs, we must zero out the non-valid portions of the
 *	page so user code sees what it expects.
 *
 *	Pages are most often semi-valid when the end of a file is mapped 
 *	into memory and the file's size is not page aligned.
 */
void
vm_page_zero_invalid(vm_page_t m, boolean_t setvalid)
{
	int b;
	int i;

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	/*
	 * Scan the valid bits looking for invalid sections that
	 * must be zerod.  Invalid sub-DEV_BSIZE'd areas ( where the
	 * valid bit may be set ) have already been zerod by
	 * vm_page_set_validclean().
	 */
	for (b = i = 0; i <= PAGE_SIZE / DEV_BSIZE; ++i) {
		if (i == (PAGE_SIZE / DEV_BSIZE) || 
		    (m->valid & ((vm_page_bits_t)1 << i))) {
			if (i > b) {
				pmap_zero_page_area(m, 
				    b << DEV_BSHIFT, (i - b) << DEV_BSHIFT);
			}
			b = i + 1;
		}
	}

	/*
	 * setvalid is TRUE when we can safely set the zero'd areas
	 * as being valid.  We can do this if there are no cache consistancy
	 * issues.  e.g. it is ok to do with UFS, but not ok to do with NFS.
	 */
	if (setvalid)
		m->valid = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_is_valid:
 *
 *	Is (partial) page valid?  Note that the case where size == 0
 *	will return FALSE in the degenerate case where the page is
 *	entirely invalid, and TRUE otherwise.
 */
int
vm_page_is_valid(vm_page_t m, int base, int size)
{
	vm_page_bits_t bits;

	VM_OBJECT_ASSERT_LOCKED(m->object);
	bits = vm_page_bits(base, size);
	return (m->valid != 0 && (m->valid & bits) == bits);
}

/*
 * Set the page's dirty bits if the page is modified.
 */
void
vm_page_test_dirty(vm_page_t m)
{

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (m->dirty != VM_PAGE_BITS_ALL && pmap_is_modified(m))
		vm_page_dirty(m);
}

void
vm_page_lock_KBI(vm_page_t m, const char *file, int line)
{

	mtx_lock_flags_(vm_page_lockptr(m), 0, file, line);
}

void
vm_page_unlock_KBI(vm_page_t m, const char *file, int line)
{

	mtx_unlock_flags_(vm_page_lockptr(m), 0, file, line);
}

int
vm_page_trylock_KBI(vm_page_t m, const char *file, int line)
{

	return (mtx_trylock_flags_(vm_page_lockptr(m), 0, file, line));
}

#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
void
vm_page_assert_locked_KBI(vm_page_t m, const char *file, int line)
{

	vm_page_lock_assert_KBI(m, MA_OWNED, file, line);
}

void
vm_page_lock_assert_KBI(vm_page_t m, int a, const char *file, int line)
{

	mtx_assert_(vm_page_lockptr(m), a, file, line);
}
#endif

#ifdef INVARIANTS
void
vm_page_object_lock_assert(vm_page_t m)
{

	/*
	 * Certain of the page's fields may only be modified by the
	 * holder of the containing object's lock or the exclusive busy.
	 * holder.  Unfortunately, the holder of the write busy is
	 * not recorded, and thus cannot be checked here.
	 */
	if (m->object != NULL && !vm_page_xbusied(m))
		VM_OBJECT_ASSERT_WLOCKED(m->object);
}
#endif

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

DB_SHOW_COMMAND(page, vm_page_print_page_info)
{
	db_printf("cnt.v_free_count: %d\n", cnt.v_free_count);
	db_printf("cnt.v_cache_count: %d\n", cnt.v_cache_count);
	db_printf("cnt.v_inactive_count: %d\n", cnt.v_inactive_count);
	db_printf("cnt.v_active_count: %d\n", cnt.v_active_count);
	db_printf("cnt.v_wire_count: %d\n", cnt.v_wire_count);
	db_printf("cnt.v_free_reserved: %d\n", cnt.v_free_reserved);
	db_printf("cnt.v_free_min: %d\n", cnt.v_free_min);
	db_printf("cnt.v_free_target: %d\n", cnt.v_free_target);
	db_printf("cnt.v_cache_min: %d\n", cnt.v_cache_min);
	db_printf("cnt.v_inactive_target: %d\n", cnt.v_inactive_target);
}

DB_SHOW_COMMAND(pageq, vm_page_print_pageq_info)
{
	int dom;

	db_printf("pq_free %d pq_cache %d\n",
	    cnt.v_free_count, cnt.v_cache_count);
	for (dom = 0; dom < vm_ndomains; dom++) {
		db_printf(
	"dom %d page_cnt %d free %d pq_act %d pq_inact %d pass %d\n",
		    dom,
		    vm_dom[dom].vmd_page_count,
		    vm_dom[dom].vmd_free_count,
		    vm_dom[dom].vmd_pagequeues[PQ_ACTIVE].pq_cnt,
		    vm_dom[dom].vmd_pagequeues[PQ_INACTIVE].pq_cnt,
		    vm_dom[dom].vmd_pass);
	}
}

DB_SHOW_COMMAND(pginfo, vm_page_print_pginfo)
{
	vm_page_t m;
	boolean_t phys;

	if (!have_addr) {
		db_printf("show pginfo addr\n");
		return;
	}

	phys = strchr(modif, 'p') != NULL;
	if (phys)
		m = PHYS_TO_VM_PAGE(addr);
	else
		m = (vm_page_t)addr;
	db_printf(
    "page %p obj %p pidx 0x%jx phys 0x%jx q %d hold %d wire %d\n"
    "  af 0x%x of 0x%x f 0x%x act %d busy %x valid 0x%x dirty 0x%x\n",
	    m, m->object, (uintmax_t)m->pindex, (uintmax_t)m->phys_addr,
	    m->queue, m->hold_count, m->wire_count, m->aflags, m->oflags,
	    m->flags, m->act_count, m->busy_lock, m->valid, m->dirty);
}
#endif /* DDB */
