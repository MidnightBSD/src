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
 *	- a pageq mutex is required when adding or removing a page from a
 *	  page queue (vm_page_queue[]), regardless of other mutexes or the
 *	  busy state of a page.
 *
 *	- The object mutex is held when inserting or removing
 *	  pages from an object (vm_page_insert() or vm_page_remove()).
 *
 */

/*
 *	Resident memory management module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
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
#include <vm/vm_reserv.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>

/*
 *	Associated with page of user-allocatable memory is a
 *	page structure.
 */

struct vpgqueues vm_page_queues[PQ_COUNT];
struct vpglocks vm_page_queue_lock;
struct vpglocks vm_page_queue_free_lock;

struct vpglocks	pa_lock[PA_LOCK_COUNT];

vm_page_t vm_page_array;
long vm_page_array_size;
long first_page;
int vm_page_zero_count;

static int boot_pages = UMA_BOOT_PAGES;
TUNABLE_INT("vm.boot_pages", &boot_pages);
SYSCTL_INT(_vm, OID_AUTO, boot_pages, CTLFLAG_RD, &boot_pages, 0,
	"number of pages allocated for bootstrapping the VM system");

int pa_tryrelock_restart;
SYSCTL_INT(_vm, OID_AUTO, tryrelock_restart, CTLFLAG_RD,
    &pa_tryrelock_restart, 0, "Number of tryrelock restarts");

static uma_zone_t fakepg_zone;

static void vm_page_clear_dirty_mask(vm_page_t m, vm_page_bits_t pagebits);
static void vm_page_queue_remove(int queue, vm_page_t m);
static void vm_page_enqueue(int queue, vm_page_t m);
static void vm_page_init_fakepg(void *dummy);

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
	 * Initialize the locks.
	 */
	mtx_init(&vm_page_queue_mtx, "vm page queue mutex", NULL, MTX_DEF |
	    MTX_RECURSE);
	mtx_init(&vm_page_queue_free_mtx, "vm page queue free mutex", NULL,
	    MTX_DEF);

	/* Setup page locks. */
	for (i = 0; i < PA_LOCK_COUNT; i++)
		mtx_init(&pa_lock[i].data, "page lock", NULL, MTX_DEF);

	/*
	 * Initialize the queue headers for the hold queue, the active queue,
	 * and the inactive queue.
	 */
	for (i = 0; i < PQ_COUNT; i++)
		TAILQ_INIT(&vm_page_queues[i].pl);
	vm_page_queues[PQ_INACTIVE].cnt = &cnt.v_inactive_count;
	vm_page_queues[PQ_ACTIVE].cnt = &cnt.v_active_count;
	vm_page_queues[PQ_HOLD].cnt = &cnt.v_active_count;

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


CTASSERT(offsetof(struct vm_page, aflags) % sizeof(uint32_t) == 0);

void
vm_page_aflag_set(vm_page_t m, uint8_t bits)
{
	uint32_t *addr, val;

	/*
	 * The PGA_WRITEABLE flag can only be set if the page is managed and
	 * VPO_BUSY.  Currently, this flag is only set by pmap_enter().
	 */
	KASSERT((bits & PGA_WRITEABLE) == 0 ||
	    (m->oflags & (VPO_UNMANAGED | VPO_BUSY)) == VPO_BUSY,
	    ("PGA_WRITEABLE and !VPO_BUSY"));

	/*
	 * We want to use atomic updates for m->aflags, which is a
	 * byte wide.  Not all architectures provide atomic operations
	 * on the single-byte destination.  Punt and access the whole
	 * 4-byte word with an atomic update.  Parallel non-atomic
	 * updates to the fields included in the update by proximity
	 * are handled properly by atomics.
	 */
	addr = (void *)&m->aflags;
	MPASS(((uintptr_t)addr & (sizeof(uint32_t) - 1)) == 0);
	val = bits;
#if BYTE_ORDER == BIG_ENDIAN
	val <<= 24;
#endif
	atomic_set_32(addr, val);
} 

void
vm_page_aflag_clear(vm_page_t m, uint8_t bits)
{
	uint32_t *addr, val;

	/*
	 * The PGA_REFERENCED flag can only be cleared if the object
	 * containing the page is locked.
	 */
	KASSERT((bits & PGA_REFERENCED) == 0 || VM_OBJECT_LOCKED(m->object),
	    ("PGA_REFERENCED and !VM_OBJECT_LOCKED"));

	/*
	 * See the comment in vm_page_aflag_set().
	 */
	addr = (void *)&m->aflags;
	MPASS(((uintptr_t)addr & (sizeof(uint32_t) - 1)) == 0);
	val = bits;
#if BYTE_ORDER == BIG_ENDIAN
	val <<= 24;
#endif
	atomic_clear_32(addr, val);
}

void
vm_page_reference(vm_page_t m)
{

	vm_page_aflag_set(m, PGA_REFERENCED);
}

void
vm_page_busy(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("vm_page_busy: page already busy!!!"));
	m->oflags |= VPO_BUSY;
}

/*
 *      vm_page_flash:
 *
 *      wakeup anyone waiting for the page.
 */
void
vm_page_flash(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->oflags & VPO_WANTED) {
		m->oflags &= ~VPO_WANTED;
		wakeup(m);
	}
}

/*
 *      vm_page_wakeup:
 *
 *      clear the VPO_BUSY flag and wakeup anyone waiting for the
 *      page.
 *
 */
void
vm_page_wakeup(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT(m->oflags & VPO_BUSY, ("vm_page_wakeup: page not busy!!!"));
	m->oflags &= ~VPO_BUSY;
	vm_page_flash(m);
}

void
vm_page_io_start(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	m->busy++;
}

void
vm_page_io_finish(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT(m->busy > 0, ("vm_page_io_finish: page %p is not busy", m));
	m->busy--;
	if (m->busy == 0)
		vm_page_flash(m);
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
	--mem->hold_count;
	KASSERT(mem->hold_count >= 0, ("vm_page_unhold: hold count < 0!!!"));
	if (mem->hold_count == 0 && mem->queue == PQ_HOLD)
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
	m->oflags = VPO_BUSY | VPO_UNMANAGED;
	m->wire_count = 1;
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
 *	vm_page_sleep:
 *
 *	Sleep and release the page and page queues locks.
 *
 *	The object containing the given page must be locked.
 */
void
vm_page_sleep(vm_page_t m, const char *msg)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (mtx_owned(&vm_page_queue_mtx))
		vm_page_unlock_queues();
	if (mtx_owned(vm_page_lockptr(m)))
		vm_page_unlock(m);

	/*
	 * It's possible that while we sleep, the page will get
	 * unbusied and freed.  If we are holding the object
	 * lock, we will assume we hold a reference to the object
	 * such that even if m->object changes, we can re-lock
	 * it.
	 */
	m->oflags |= VPO_WANTED;
	msleep(m, VM_OBJECT_MTX(m->object), PVM, msg, 0);
}

/*
 *	vm_page_dirty:
 *
 *	Set all bits in the page's dirty field.
 *
 *	The object containing the specified page must be locked if the
 *	call is made from the machine-independent layer.
 *
 *	See vm_page_clear_dirty_mask().
 */
void
vm_page_dirty(vm_page_t m)
{

	KASSERT((m->flags & PG_CACHED) == 0,
	    ("vm_page_dirty: page in cache!"));
	KASSERT(!VM_PAGE_IS_FREE(m),
	    ("vm_page_dirty: page is free!"));
	KASSERT(m->valid == VM_PAGE_BITS_ALL,
	    ("vm_page_dirty: page is invalid!"));
	m->dirty = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_splay:
 *
 *	Implements Sleator and Tarjan's top-down splay algorithm.  Returns
 *	the vm_page containing the given pindex.  If, however, that
 *	pindex is not found in the vm_object, returns a vm_page that is
 *	adjacent to the pindex, coming before or after it.
 */
vm_page_t
vm_page_splay(vm_pindex_t pindex, vm_page_t root)
{
	struct vm_page dummy;
	vm_page_t lefttreemax, righttreemin, y;

	if (root == NULL)
		return (root);
	lefttreemax = righttreemin = &dummy;
	for (;; root = y) {
		if (pindex < root->pindex) {
			if ((y = root->left) == NULL)
				break;
			if (pindex < y->pindex) {
				/* Rotate right. */
				root->left = y->right;
				y->right = root;
				root = y;
				if ((y = root->left) == NULL)
					break;
			}
			/* Link into the new root's right tree. */
			righttreemin->left = root;
			righttreemin = root;
		} else if (pindex > root->pindex) {
			if ((y = root->right) == NULL)
				break;
			if (pindex > y->pindex) {
				/* Rotate left. */
				root->right = y->left;
				y->left = root;
				root = y;
				if ((y = root->right) == NULL)
					break;
			}
			/* Link into the new root's left tree. */
			lefttreemax->right = root;
			lefttreemax = root;
		} else
			break;
	}
	/* Assemble the new root. */
	lefttreemax->right = root->left;
	righttreemin->left = root->right;
	root->left = dummy.right;
	root->right = dummy.left;
	return (root);
}

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object and object list.
 *
 *	The pagetables are not updated but will presumably fault the page
 *	in if necessary, or if a kernel page the caller will at some point
 *	enter the page into the kernel's pmap.  We are not allowed to block
 *	here so we *can't* do this anyway.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
void
vm_page_insert(vm_page_t m, vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t root;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (m->object != NULL)
		panic("vm_page_insert: page already inserted");

	/*
	 * Record the object/offset pair in this page
	 */
	m->object = object;
	m->pindex = pindex;

	/*
	 * Now link into the object's ordered list of backed pages.
	 */
	root = object->root;
	if (root == NULL) {
		m->left = NULL;
		m->right = NULL;
		TAILQ_INSERT_TAIL(&object->memq, m, listq);
	} else {
		root = vm_page_splay(pindex, root);
		if (pindex < root->pindex) {
			m->left = root->left;
			m->right = root;
			root->left = NULL;
			TAILQ_INSERT_BEFORE(root, m, listq);
		} else if (pindex == root->pindex)
			panic("vm_page_insert: offset already allocated");
		else {
			m->right = root->right;
			m->left = root;
			root->right = NULL;
			TAILQ_INSERT_AFTER(&object->memq, root, m, listq);
		}
	}
	object->root = m;

	/*
	 * show that the object has one more resident page.
	 */
	object->resident_page_count++;
	/*
	 * Hold the vnode until the last page is released.
	 */
	if (object->resident_page_count == 1 && object->type == OBJT_VNODE)
		vhold((struct vnode *)object->handle);

	/*
	 * Since we are inserting a new and possibly dirty page,
	 * update the object's OBJ_MIGHTBEDIRTY flag.
	 */
	if (m->aflags & PGA_WRITEABLE)
		vm_object_set_writeable_dirty(object);
}

/*
 *	vm_page_remove:
 *				NOTE: used by device pager as well -wfj
 *
 *	Removes the given mem entry from the object/offset-page
 *	table and the object page list, but do not invalidate/terminate
 *	the backing store.
 *
 *	The object and page must be locked.
 *	The underlying pmap entry (if any) is NOT removed here.
 *	This routine may not block.
 */
void
vm_page_remove(vm_page_t m)
{
	vm_object_t object;
	vm_page_t next, prev, root;

	if ((m->oflags & VPO_UNMANAGED) == 0)
		vm_page_lock_assert(m, MA_OWNED);
	if ((object = m->object) == NULL)
		return;
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (m->oflags & VPO_BUSY) {
		m->oflags &= ~VPO_BUSY;
		vm_page_flash(m);
	}

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if ((next = TAILQ_NEXT(m, listq)) != NULL && next->left == m) {
		/*
		 * Since the page's successor in the list is also its parent
		 * in the tree, its right subtree must be empty.
		 */
		next->left = m->left;
		KASSERT(m->right == NULL,
		    ("vm_page_remove: page %p has right child", m));
	} else if ((prev = TAILQ_PREV(m, pglist, listq)) != NULL &&
	    prev->right == m) {
		/*
		 * Since the page's predecessor in the list is also its parent
		 * in the tree, its left subtree must be empty.
		 */
		KASSERT(m->left == NULL,
		    ("vm_page_remove: page %p has left child", m));
		prev->right = m->right;
	} else {
		if (m != object->root)
			vm_page_splay(m->pindex, object->root);
		if (m->left == NULL)
			root = m->right;
		else if (m->right == NULL)
			root = m->left;
		else {
			/*
			 * Move the page's successor to the root, because
			 * pages are usually removed in ascending order.
			 */
			if (m->right != next)
				vm_page_splay(m->pindex, m->right);
			next->left = m->left;
			root = next;
		}
		object->root = root;
	}
	TAILQ_REMOVE(&object->memq, m, listq);

	/*
	 * And show that the object has one fewer resident page.
	 */
	object->resident_page_count--;
	/*
	 * The vnode may now be recycled.
	 */
	if (object->resident_page_count == 0 && object->type == OBJT_VNODE)
		vdrop((struct vnode *)object->handle);

	m->object = NULL;
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 *	This routine may not block.
 *	This is a critical path routine
 */
vm_page_t
vm_page_lookup(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if ((m = object->root) != NULL && m->pindex != pindex) {
		m = vm_page_splay(pindex, m);
		if ((object->root = m)->pindex != pindex)
			m = NULL;
	}
	return (m);
}

/*
 *	vm_page_find_least:
 *
 *	Returns the page associated with the object with least pindex
 *	greater than or equal to the parameter pindex, or NULL.
 *
 *	The object must be locked.
 *	The routine may not block.
 */
vm_page_t
vm_page_find_least(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if ((m = TAILQ_FIRST(&object->memq)) != NULL) {
		if (m->pindex < pindex) {
			m = vm_page_splay(pindex, object->root);
			if ((object->root = m)->pindex < pindex)
				m = TAILQ_NEXT(m, listq);
		}
	}
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

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((prev = TAILQ_PREV(m, pglist, listq)) != NULL &&
	    prev->pindex != m->pindex - 1)
		prev = NULL;
	return (prev);
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	The object must be locked.
 *	This routine may not block.
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
 */
void
vm_page_rename(vm_page_t m, vm_object_t new_object, vm_pindex_t new_pindex)
{

	vm_page_remove(m);
	vm_page_insert(m, new_object, new_pindex);
	vm_page_dirty(m);
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
	vm_page_t m, m_next;
	boolean_t empty;

	mtx_lock(&vm_page_queue_free_mtx);
	if (__predict_false(object->cache == NULL)) {
		mtx_unlock(&vm_page_queue_free_mtx);
		return;
	}
	m = object->cache = vm_page_splay(start, object->cache);
	if (m->pindex < start) {
		if (m->right == NULL)
			m = NULL;
		else {
			m_next = vm_page_splay(start, m->right);
			m_next->left = m;
			m->right = NULL;
			m = object->cache = m_next;
		}
	}

	/*
	 * At this point, "m" is either (1) a reference to the page
	 * with the least pindex that is greater than or equal to
	 * "start" or (2) NULL.
	 */
	for (; m != NULL && (m->pindex < end || end == 0); m = m_next) {
		/*
		 * Find "m"'s successor and remove "m" from the
		 * object's cache.
		 */
		if (m->right == NULL) {
			object->cache = m->left;
			m_next = NULL;
		} else {
			m_next = vm_page_splay(start, m->right);
			m_next->left = m->left;
			object->cache = m_next;
		}
		/* Convert "m" to a free page. */
		m->object = NULL;
		m->valid = 0;
		/* Clear PG_CACHED and set PG_FREE. */
		m->flags ^= PG_CACHED | PG_FREE;
		KASSERT((m->flags & (PG_CACHED | PG_FREE)) == PG_FREE,
		    ("vm_page_cache_free: page %p has inconsistent flags", m));
		cnt.v_cache_count--;
		cnt.v_free_count++;
	}
	empty = object->cache == NULL;
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
	vm_page_t m;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	if ((m = object->cache) != NULL && m->pindex != pindex) {
		m = vm_page_splay(pindex, m);
		if ((object->cache = m)->pindex != pindex)
			m = NULL;
	}
	return (m);
}

/*
 *	Remove the given cached page from its containing object's
 *	collection of cached pages.
 *
 *	The free page queue must be locked.
 */
void
vm_page_cache_remove(vm_page_t m)
{
	vm_object_t object;
	vm_page_t root;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	KASSERT((m->flags & PG_CACHED) != 0,
	    ("vm_page_cache_remove: page %p is not cached", m));
	object = m->object;
	if (m != object->cache) {
		root = vm_page_splay(m->pindex, object->cache);
		KASSERT(root == m,
		    ("vm_page_cache_remove: page %p is not cached in object %p",
		    m, object));
	}
	if (m->left == NULL)
		root = m->right;
	else if (m->right == NULL)
		root = m->left;
	else {
		root = vm_page_splay(m->pindex, m->left);
		root->right = m->right;
	}
	object->cache = root;
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
	vm_page_t m, m_next;

	/*
	 * Insertion into an object's collection of cached pages
	 * requires the object to be locked.  In contrast, removal does
	 * not.
	 */
	VM_OBJECT_LOCK_ASSERT(new_object, MA_OWNED);
	KASSERT(new_object->cache == NULL,
	    ("vm_page_cache_transfer: object %p has cached pages",
	    new_object));
	mtx_lock(&vm_page_queue_free_mtx);
	if ((m = orig_object->cache) != NULL) {
		/*
		 * Transfer all of the pages with offset greater than or
		 * equal to 'offidxstart' from the original object's
		 * cache to the new object's cache.
		 */
		m = vm_page_splay(offidxstart, m);
		if (m->pindex < offidxstart) {
			orig_object->cache = m;
			new_object->cache = m->right;
			m->right = NULL;
		} else {
			orig_object->cache = m->left;
			new_object->cache = m;
			m->left = NULL;
		}
		while ((m = new_object->cache) != NULL) {
			if ((m->pindex - offidxstart) >= new_object->size) {
				/*
				 * Return all of the cached pages with
				 * offset greater than or equal to the
				 * new object's size to the original
				 * object's cache. 
				 */
				new_object->cache = m->left;
				m->left = orig_object->cache;
				orig_object->cache = m;
				break;
			}
			m_next = vm_page_splay(m->pindex, m->right);
			/* Update the page's object and offset. */
			m->object = new_object;
			m->pindex -= offidxstart;
			if (m_next == NULL)
				break;
			m->right = NULL;
			m_next->left = m;
			new_object->cache = m_next;
		}
		KASSERT(new_object->cache == NULL ||
		    new_object->type == OBJT_SWAP,
		    ("vm_page_cache_transfer: object %p's type is incompatible"
		    " with cached pages", new_object));
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
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->cache == NULL)
		return (FALSE);
	mtx_lock(&vm_page_queue_free_mtx);
	m = vm_page_cache_lookup(object, pindex);
	mtx_unlock(&vm_page_queue_free_mtx);
	return (m != NULL);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a memory cell associated
 *	with this VM object/offset pair.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_ZERO		prefer a zeroed page
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_NOOBJ		page is not associated with a vm object
 *	VM_ALLOC_NOBUSY		do not set the page busy
 *	VM_ALLOC_IFCACHED	return page only if it is cached
 *	VM_ALLOC_IFNOTCACHED	return NULL, do not reactivate if the page
 *				is cached
 *
 *	This routine may not sleep.
 */
vm_page_t
vm_page_alloc(vm_object_t object, vm_pindex_t pindex, int req)
{
	struct vnode *vp = NULL;
	vm_object_t m_object;
	vm_page_t m;
	int flags, page_req;

	if ((req & VM_ALLOC_NOOBJ) == 0) {
		KASSERT(object != NULL,
		    ("vm_page_alloc: NULL object."));
		VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	}

	page_req = req & VM_ALLOC_CLASS_MASK;

	/*
	 * The pager is allowed to eat deeper into the free page list.
	 */
	if ((curproc == pageproc) && (page_req != VM_ALLOC_INTERRUPT))
		page_req = VM_ALLOC_SYSTEM;

	mtx_lock(&vm_page_queue_free_mtx);
	if (cnt.v_free_count + cnt.v_cache_count > cnt.v_free_reserved ||
	    (page_req == VM_ALLOC_SYSTEM && 
	    cnt.v_free_count + cnt.v_cache_count > cnt.v_interrupt_free_min) ||
	    (page_req == VM_ALLOC_INTERRUPT &&
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
		} else if (object == NULL || object->type == OBJT_DEVICE ||
		    object->type == OBJT_SG ||
		    (object->flags & OBJ_COLORED) == 0 ||
		    (m = vm_reserv_alloc_page(object, pindex)) == NULL) {
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
		    MAX((u_int)req >> VM_ALLOC_COUNT_SHIFT, 1));
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
	KASSERT(m->busy == 0, ("vm_page_alloc: page %p is busy", m));
	KASSERT(m->dirty == 0, ("vm_page_alloc: page %p is dirty", m));
	KASSERT(pmap_page_get_memattr(m) == VM_MEMATTR_DEFAULT,
	    ("vm_page_alloc: page %p has unexpected memattr %d", m,
	    pmap_page_get_memattr(m)));
	if ((m->flags & PG_CACHED) != 0) {
		KASSERT(m->valid != 0,
		    ("vm_page_alloc: cached page %p is invalid", m));
		if (m->object == object && m->pindex == pindex)
	  		cnt.v_reactivated++;
		else
			m->valid = 0;
		m_object = m->object;
		vm_page_cache_remove(m);
		if (m_object->type == OBJT_VNODE && m_object->cache == NULL)
			vp = m_object->handle;
	} else {
		KASSERT(VM_PAGE_IS_FREE(m),
		    ("vm_page_alloc: page %p is not free", m));
		KASSERT(m->valid == 0,
		    ("vm_page_alloc: free page %p is valid", m));
		cnt.v_free_count--;
	}

	/*
	 * Only the PG_ZERO flag is inherited.  The PG_CACHED or PG_FREE flag
	 * must be cleared before the free page queues lock is released.
	 */
	flags = 0;
	if (req & VM_ALLOC_NODUMP)
		flags |= PG_NODUMP;
	if (m->flags & PG_ZERO) {
		vm_page_zero_count--;
		if (req & VM_ALLOC_ZERO)
			flags = PG_ZERO;
	}
	m->flags = flags;
	mtx_unlock(&vm_page_queue_free_mtx);
	m->aflags = 0;
	if (object == NULL || object->type == OBJT_PHYS)
		m->oflags = VPO_UNMANAGED;
	else
		m->oflags = 0;
	if ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_NOOBJ)) == 0)
		m->oflags |= VPO_BUSY;
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
		/* Ignore device objects; the pager sets "memattr" for them. */
		if (object->memattr != VM_MEMATTR_DEFAULT &&
		    object->type != OBJT_DEVICE && object->type != OBJT_SG)
			pmap_page_set_memattr(m, object->memattr);
		vm_page_insert(m, object, pindex);
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

/*
 * Initialize a page that has been freshly dequeued from a freelist.
 * The caller has to drop the vnode returned, if it is not NULL.
 *
 * To be called with vm_page_queue_free_mtx held.
 */
struct vnode *
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
	KASSERT(m->busy == 0,
	    ("vm_page_alloc_init: page %p is busy", m));
	KASSERT(m->dirty == 0,
	    ("vm_page_alloc_init: page %p is dirty", m));
	KASSERT(pmap_page_get_memattr(m) == VM_MEMATTR_DEFAULT,
	    ("vm_page_alloc_init: page %p has unexpected memattr %d",
	    m, pmap_page_get_memattr(m)));
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	drop = NULL;
	if ((m->flags & PG_CACHED) != 0) {
		m->valid = 0;
		m_object = m->object;
		vm_page_cache_remove(m);
		if (m_object->type == OBJT_VNODE &&
		    m_object->cache == NULL)
			drop = m_object->handle;
	} else {
		KASSERT(VM_PAGE_IS_FREE(m),
		    ("vm_page_alloc_init: page %p is not free", m));
		KASSERT(m->valid == 0,
		    ("vm_page_alloc_init: free page %p is valid", m));
		cnt.v_free_count--;
	}
	if (m->flags & PG_ZERO)
		vm_page_zero_count--;
	/* Don't clear the PG_ZERO flag; we'll need it later. */
	m->flags &= PG_ZERO;
	m->aflags = 0;
	m->oflags = VPO_UNMANAGED;
	/* Unmanaged pages don't use "act_count". */
	return (drop);
}

/*
 * 	vm_page_alloc_freelist:
 * 
 *	Allocate a page from the specified freelist.
 *	Only the ALLOC_CLASS values in req are honored, other request flags
 *	are ignored.
 */
vm_page_t
vm_page_alloc_freelist(int flind, int req)
{
	struct vnode *drop;
	vm_page_t m;
	int page_req;

	m = NULL;
	page_req = req & VM_ALLOC_CLASS_MASK;
	mtx_lock(&vm_page_queue_free_mtx);
	/*
	 * Do not allocate reserved pages unless the req has asked for it.
	 */
	if (cnt.v_free_count + cnt.v_cache_count > cnt.v_free_reserved ||
	    (page_req == VM_ALLOC_SYSTEM && 
	    cnt.v_free_count + cnt.v_cache_count > cnt.v_interrupt_free_min) ||
	    (page_req == VM_ALLOC_INTERRUPT &&
	    cnt.v_free_count + cnt.v_cache_count > 0)) {
		m = vm_phys_alloc_freelist_pages(flind, VM_FREEPOOL_DIRECT, 0);
	}
	if (m == NULL) {
		mtx_unlock(&vm_page_queue_free_mtx);
		return (NULL);
	}
	drop = vm_page_alloc_init(m);
	mtx_unlock(&vm_page_queue_free_mtx);
	if (drop)
		vdrop(drop);
	return (m);
}

/*
 *	vm_wait:	(also see VM_WAIT macro)
 *
 *	Block until free pages are available for allocation
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
 *	Block until free pages are available for allocation
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

/*
 *	vm_page_requeue:
 *
 *	Move the given page to the tail of its present page queue.
 *
 *	The page queues must be locked.
 */
void
vm_page_requeue(vm_page_t m)
{
	struct vpgqueues *vpq;
	int queue;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	queue = m->queue;
	KASSERT(queue != PQ_NONE,
	    ("vm_page_requeue: page %p is not queued", m));
	vpq = &vm_page_queues[queue];
	TAILQ_REMOVE(&vpq->pl, m, pageq);
	TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
}

/*
 *	vm_page_queue_remove:
 *
 *	Remove the given page from the specified queue.
 *
 *	The page and page queues must be locked.
 */
static __inline void
vm_page_queue_remove(int queue, vm_page_t m)
{
	struct vpgqueues *pq;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	vm_page_lock_assert(m, MA_OWNED);
	pq = &vm_page_queues[queue];
	TAILQ_REMOVE(&pq->pl, m, pageq);
	(*pq->cnt)--;
}

/*
 *	vm_pageq_remove:
 *
 *	Remove a page from its queue.
 *
 *	The given page must be locked.
 *	This routine may not block.
 */
void
vm_pageq_remove(vm_page_t m)
{
	int queue;

	vm_page_lock_assert(m, MA_OWNED);
	if ((queue = m->queue) != PQ_NONE) {
		vm_page_lock_queues();
		m->queue = PQ_NONE;
		vm_page_queue_remove(queue, m);
		vm_page_unlock_queues();
	}
}

/*
 *	vm_page_enqueue:
 *
 *	Add the given page to the specified queue.
 *
 *	The page queues must be locked.
 */
static void
vm_page_enqueue(int queue, vm_page_t m)
{
	struct vpgqueues *vpq;

	vpq = &vm_page_queues[queue];
	m->queue = queue;
	TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
	++*vpq->cnt;
}

/*
 *	vm_page_activate:
 *
 *	Put the specified page on the active list (if appropriate).
 *	Ensure that act_count is at least ACT_INIT but do not otherwise
 *	mess with it.
 *
 *	The page must be locked.
 *	This routine may not block.
 */
void
vm_page_activate(vm_page_t m)
{
	int queue;

	vm_page_lock_assert(m, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((queue = m->queue) != PQ_ACTIVE) {
		if (m->wire_count == 0 && (m->oflags & VPO_UNMANAGED) == 0) {
			if (m->act_count < ACT_INIT)
				m->act_count = ACT_INIT;
			vm_page_lock_queues();
			if (queue != PQ_NONE)
				vm_page_queue_remove(queue, m);
			vm_page_enqueue(PQ_ACTIVE, m);
			vm_page_unlock_queues();
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
 *	This routine may not block.
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
 *	vm_page_free_toq:
 *
 *	Returns the given page to the free list,
 *	disassociating it with any VM object.
 *
 *	Object and page must be locked prior to entry.
 *	This routine may not block.
 */

void
vm_page_free_toq(vm_page_t m)
{

	if ((m->oflags & VPO_UNMANAGED) == 0) {
		vm_page_lock_assert(m, MA_OWNED);
		KASSERT(!pmap_page_is_mapped(m),
		    ("vm_page_free_toq: freeing mapped page %p", m));
	}
	PCPU_INC(cnt.v_tfree);

	if (VM_PAGE_IS_FREE(m))
		panic("vm_page_free: freeing free page %p", m);
	else if (m->busy != 0)
		panic("vm_page_free: freeing busy page %p", m);

	/*
	 * unqueue, then remove page.  Note that we cannot destroy
	 * the page here because we do not want to call the pager's
	 * callback routine until after we've put the page on the
	 * appropriate free queue.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0)
		vm_pageq_remove(m);
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
		vm_page_lock_queues();
		vm_page_enqueue(PQ_HOLD, m);
		vm_page_unlock_queues();
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
		cnt.v_free_count++;
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
 *	This routine may not block.
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
		if ((m->oflags & VPO_UNMANAGED) == 0)
			vm_pageq_remove(m);
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
 * If a page is fictitious, then its wire count must alway be one.
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
			vm_page_lock_queues();
			vm_page_enqueue(activate ? PQ_ACTIVE : PQ_INACTIVE, m);
			vm_page_unlock_queues();
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
 * This routine may not block.
 */
static inline void
_vm_page_deactivate(vm_page_t m, int athead)
{
	int queue;

	vm_page_lock_assert(m, MA_OWNED);

	/*
	 * Ignore if already inactive.
	 */
	if ((queue = m->queue) == PQ_INACTIVE)
		return;
	if (m->wire_count == 0 && (m->oflags & VPO_UNMANAGED) == 0) {
		m->flags &= ~PG_WINATCFLS;
		vm_page_lock_queues();
		if (queue != PQ_NONE)
			vm_page_queue_remove(queue, m);
		if (athead)
			TAILQ_INSERT_HEAD(&vm_page_queues[PQ_INACTIVE].pl, m,
			    pageq);
		else
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m,
			    pageq);
		m->queue = PQ_INACTIVE;
		cnt.v_inactive_count++;
		vm_page_unlock_queues();
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
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->dirty || m->hold_count || m->busy || m->wire_count ||
	    (m->oflags & (VPO_BUSY | VPO_UNMANAGED)) != 0)
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
		VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->dirty || m->hold_count || m->busy || m->wire_count ||
	    (m->oflags & (VPO_BUSY | VPO_UNMANAGED)) != 0)
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
 * This routine may not block.
 */
void
vm_page_cache(vm_page_t m)
{
	vm_object_t object;
	vm_page_t next, prev, root;

	vm_page_lock_assert(m, MA_OWNED);
	object = m->object;
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if ((m->oflags & (VPO_UNMANAGED | VPO_BUSY)) || m->busy ||
	    m->hold_count || m->wire_count)
		panic("vm_page_cache: attempting to cache busy page");
	pmap_remove_all(m);
	if (m->dirty != 0)
		panic("vm_page_cache: page %p is dirty", m);
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
	PCPU_INC(cnt.v_tcached);

	/*
	 * Remove the page from the paging queues.
	 */
	vm_pageq_remove(m);

	/*
	 * Remove the page from the object's collection of resident
	 * pages. 
	 */
	if ((next = TAILQ_NEXT(m, listq)) != NULL && next->left == m) {
		/*
		 * Since the page's successor in the list is also its parent
		 * in the tree, its right subtree must be empty.
		 */
		next->left = m->left;
		KASSERT(m->right == NULL,
		    ("vm_page_cache: page %p has right child", m));
	} else if ((prev = TAILQ_PREV(m, pglist, listq)) != NULL &&
	    prev->right == m) {
		/*
		 * Since the page's predecessor in the list is also its parent
		 * in the tree, its left subtree must be empty.
		 */
		KASSERT(m->left == NULL,
		    ("vm_page_cache: page %p has left child", m));
		prev->right = m->right;
	} else {
		if (m != object->root)
			vm_page_splay(m->pindex, object->root);
		if (m->left == NULL)
			root = m->right;
		else if (m->right == NULL)
			root = m->left;
		else {
			/*
			 * Move the page's successor to the root, because
			 * pages are usually removed in ascending order.
			 */
			if (m->right != next)
				vm_page_splay(m->pindex, m->right);
			next->left = m->left;
			root = next;
		}
		object->root = root;
	}
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
	m->flags |= PG_CACHED;
	cnt.v_cache_count++;
	root = object->cache;
	if (root == NULL) {
		m->left = NULL;
		m->right = NULL;
	} else {
		root = vm_page_splay(m->pindex, root);
		if (m->pindex < root->pindex) {
			m->left = root->left;
			m->right = root;
			root->left = NULL;
		} else if (__predict_false(m->pindex == root->pindex))
			panic("vm_page_cache: offset already cached");
		else {
			m->right = root->right;
			m->left = root;
			root->right = NULL;
		}
	}
	object->cache = m;
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
		if (root == NULL && object->resident_page_count != 0)
			vhold(object->handle);
		else if (root != NULL && object->resident_page_count == 0)
			vdrop(object->handle);
	}
}

/*
 * vm_page_dontneed
 *
 *	Cache, deactivate, or do nothing as appropriate.  This routine
 *	is typically used by madvise() MADV_DONTNEED.
 *
 *	Generally speaking we want to move the page into the cache so
 *	it gets reused quickly.  However, this can result in a silly syndrome
 *	due to the page recycling too quickly.  Small objects will not be
 *	fully cached.  On the otherhand, if we move the page to the inactive
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
 */
void
vm_page_dontneed(vm_page_t m)
{
	int dnw;
	int head;

	vm_page_lock_assert(m, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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
	 *
	 * Perform the pmap_clear_reference() first.  Otherwise, a concurrent
	 * pmap operation, such as pmap_remove(), could clear a reference in
	 * the pmap and set PGA_REFERENCED on the page before the
	 * pmap_clear_reference() had completed.  Consequently, the page would
	 * appear referenced based upon an old reference that occurred before
	 * this function ran.
	 */
	pmap_clear_reference(m);
	vm_page_aflag_clear(m, PGA_REFERENCED);

	if (m->dirty == 0 && pmap_is_modified(m))
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
 * The caller must always specify the VM_ALLOC_RETRY flag.  This is intended
 * to facilitate its eventual removal.
 *
 * This routine may block.
 */
vm_page_t
vm_page_grab(vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT((allocflags & VM_ALLOC_RETRY) != 0,
	    ("vm_page_grab: VM_ALLOC_RETRY is required"));
retrylookup:
	if ((m = vm_page_lookup(object, pindex)) != NULL) {
		if ((m->oflags & VPO_BUSY) != 0 ||
		    ((allocflags & VM_ALLOC_IGN_SBUSY) == 0 && m->busy != 0)) {
			/*
			 * Reference the page before unlocking and
			 * sleeping so that the page daemon is less
			 * likely to reclaim it.
			 */
			vm_page_aflag_set(m, PGA_REFERENCED);
			vm_page_sleep(m, "pgrbwt");
			goto retrylookup;
		} else {
			if ((allocflags & VM_ALLOC_WIRED) != 0) {
				vm_page_lock(m);
				vm_page_wire(m);
				vm_page_unlock(m);
			}
			if ((allocflags & VM_ALLOC_NOBUSY) == 0)
				vm_page_busy(m);
			return (m);
		}
	}
	m = vm_page_alloc(object, pindex, allocflags & ~(VM_ALLOC_RETRY |
	    VM_ALLOC_IGN_SBUSY));
	if (m == NULL) {
		VM_OBJECT_UNLOCK(object);
		VM_WAIT;
		VM_OBJECT_LOCK(object);
		goto retrylookup;
	} else if (m->valid != 0)
		return (m);
	if (allocflags & VM_ALLOC_ZERO && (m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	return (m);
}

/*
 * Mapping function for valid bits or for dirty bits in
 * a page.  May not block.
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
 *	vm_page_set_valid:
 *
 *	Sets portions of a page valid.  The arguments are expected
 *	to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 *	of any partial chunks touched by the range.  The invalid portion of
 *	such chunks will be zeroed.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_valid(vm_page_t m, int base, int size)
{
	int endoff, frag;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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
	    ("vm_page_set_valid: page %p is dirty", m)); 

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
	 * If the object is locked and the page is neither VPO_BUSY nor
	 * PGA_WRITEABLE, then the page's dirty field cannot possibly be
	 * set by a concurrent pmap operation.
	 */
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->oflags & VPO_BUSY) == 0 && (m->aflags & PGA_WRITEABLE) == 0)
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
 *	This routine may not block.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_validclean(vm_page_t m, int base, int size)
{
	vm_page_bits_t oldvalid, pagebits;
	int endoff, frag;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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
 *
 *	May not block.
 */
void
vm_page_set_invalid(vm_page_t m, int base, int size)
{
	vm_page_bits_t bits;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("vm_page_set_invalid: page %p is busy", m));
	bits = vm_page_bits(base, size);
	if (m->valid == VM_PAGE_BITS_ALL && bits != 0)
		pmap_remove_all(m);
	KASSERT(!pmap_page_is_mapped(m),
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

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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
 *
 *	May not block.
 */
int
vm_page_is_valid(vm_page_t m, int base, int size)
{
	vm_page_bits_t bits;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	bits = vm_page_bits(base, size);
	if (m->valid && ((m->valid & bits) == bits))
		return 1;
	else
		return 0;
}

/*
 * update dirty bits from pmap/mmu.  May not block.
 */
void
vm_page_test_dirty(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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
vm_page_lock_assert_KBI(vm_page_t m, int a, const char *file, int line)
{

	mtx_assert_(vm_page_lockptr(m), a, file, line);
}
#endif

int so_zerocp_fullpage = 0;

/*
 *	Replace the given page with a copy.  The copied page assumes
 *	the portion of the given page's "wire_count" that is not the
 *	responsibility of this copy-on-write mechanism.
 *
 *	The object containing the given page must have a non-zero
 *	paging-in-progress count and be locked.
 */
void
vm_page_cowfault(vm_page_t m)
{
	vm_page_t mnew;
	vm_object_t object;
	vm_pindex_t pindex;

	mtx_assert(&vm_page_queue_mtx, MA_NOTOWNED);
	vm_page_lock_assert(m, MA_OWNED);
	object = m->object;
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT(object->paging_in_progress != 0,
	    ("vm_page_cowfault: object %p's paging-in-progress count is zero.",
	    object)); 
	pindex = m->pindex;

 retry_alloc:
	pmap_remove_all(m);
	vm_page_remove(m);
	mnew = vm_page_alloc(object, pindex, VM_ALLOC_NORMAL | VM_ALLOC_NOBUSY);
	if (mnew == NULL) {
		vm_page_insert(m, object, pindex);
		vm_page_unlock(m);
		VM_OBJECT_UNLOCK(object);
		VM_WAIT;
		VM_OBJECT_LOCK(object);
		if (m == vm_page_lookup(object, pindex)) {
			vm_page_lock(m);
			goto retry_alloc;
		} else {
			/*
			 * Page disappeared during the wait.
			 */
			return;
		}
	}

	if (m->cow == 0) {
		/* 
		 * check to see if we raced with an xmit complete when 
		 * waiting to allocate a page.  If so, put things back 
		 * the way they were 
		 */
		vm_page_unlock(m);
		vm_page_lock(mnew);
		vm_page_free(mnew);
		vm_page_unlock(mnew);
		vm_page_insert(m, object, pindex);
	} else { /* clear COW & copy page */
		if (!so_zerocp_fullpage)
			pmap_copy_page(m, mnew);
		mnew->valid = VM_PAGE_BITS_ALL;
		vm_page_dirty(mnew);
		mnew->wire_count = m->wire_count - m->cow;
		m->wire_count = m->cow;
		vm_page_unlock(m);
	}
}

void 
vm_page_cowclear(vm_page_t m)
{

	vm_page_lock_assert(m, MA_OWNED);
	if (m->cow) {
		m->cow--;
		/* 
		 * let vm_fault add back write permission  lazily
		 */
	} 
	/*
	 *  sf_buf_free() will free the page, so we needn't do it here
	 */ 
}

int
vm_page_cowsetup(vm_page_t m)
{

	vm_page_lock_assert(m, MA_OWNED);
	if ((m->flags & PG_FICTITIOUS) != 0 ||
	    (m->oflags & VPO_UNMANAGED) != 0 ||
	    m->cow == USHRT_MAX - 1 || !VM_OBJECT_TRYLOCK(m->object))
		return (EBUSY);
	m->cow++;
	pmap_remove_write(m);
	VM_OBJECT_UNLOCK(m->object);
	return (0);
}

#ifdef INVARIANTS
void
vm_page_object_lock_assert(vm_page_t m)
{

	/*
	 * Certain of the page's fields may only be modified by the
	 * holder of the containing object's lock or the setter of the
	 * page's VPO_BUSY flag.  Unfortunately, the setter of the
	 * VPO_BUSY flag is not recorded, and thus cannot be checked
	 * here.
	 */
	if (m->object != NULL && (m->oflags & VPO_BUSY) == 0)
		VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
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
		
	db_printf("PQ_FREE:");
	db_printf(" %d", cnt.v_free_count);
	db_printf("\n");
		
	db_printf("PQ_CACHE:");
	db_printf(" %d", cnt.v_cache_count);
	db_printf("\n");

	db_printf("PQ_ACTIVE: %d, PQ_INACTIVE: %d\n",
		*vm_page_queues[PQ_ACTIVE].cnt,
		*vm_page_queues[PQ_INACTIVE].cnt);
}
#endif /* DDB */
