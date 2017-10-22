/*-
 * Copyright (c) 2002-2006 Rice University
 * Copyright (c) 2007 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Alan L. Cox,
 * Olivier Crameri, Peter Druschel, Sitaram Iyer, and Juan Navarro.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_reserv.h>

/*
 * VM_FREELIST_DEFAULT is split into VM_NDOMAIN lists, one for each
 * domain.  These extra lists are stored at the end of the regular
 * free lists starting with VM_NFREELIST.
 */
#define VM_RAW_NFREELIST	(VM_NFREELIST + VM_NDOMAIN - 1)

struct vm_freelist {
	struct pglist pl;
	int lcnt;
};

struct vm_phys_seg {
	vm_paddr_t	start;
	vm_paddr_t	end;
	vm_page_t	first_page;
	int		domain;
	struct vm_freelist (*free_queues)[VM_NFREEPOOL][VM_NFREEORDER];
};

struct mem_affinity *mem_affinity;

static struct vm_phys_seg vm_phys_segs[VM_PHYSSEG_MAX];

static int vm_phys_nsegs;

#define VM_PHYS_FICTITIOUS_NSEGS	8
static struct vm_phys_fictitious_seg {
	vm_paddr_t	start;
	vm_paddr_t	end;
	vm_page_t	first_page;
} vm_phys_fictitious_segs[VM_PHYS_FICTITIOUS_NSEGS];
static struct mtx vm_phys_fictitious_reg_mtx;
MALLOC_DEFINE(M_FICT_PAGES, "", "");

static struct vm_freelist
    vm_phys_free_queues[VM_RAW_NFREELIST][VM_NFREEPOOL][VM_NFREEORDER];
static struct vm_freelist
(*vm_phys_lookup_lists[VM_NDOMAIN][VM_RAW_NFREELIST])[VM_NFREEPOOL][VM_NFREEORDER];

static int vm_nfreelists = VM_FREELIST_DEFAULT + 1;

static int cnt_prezero;
SYSCTL_INT(_vm_stats_misc, OID_AUTO, cnt_prezero, CTLFLAG_RD,
    &cnt_prezero, 0, "The number of physical pages prezeroed at idle time");

static int sysctl_vm_phys_free(SYSCTL_HANDLER_ARGS);
SYSCTL_OID(_vm, OID_AUTO, phys_free, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, sysctl_vm_phys_free, "A", "Phys Free Info");

static int sysctl_vm_phys_segs(SYSCTL_HANDLER_ARGS);
SYSCTL_OID(_vm, OID_AUTO, phys_segs, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, sysctl_vm_phys_segs, "A", "Phys Seg Info");

#if VM_NDOMAIN > 1
static int sysctl_vm_phys_lookup_lists(SYSCTL_HANDLER_ARGS);
SYSCTL_OID(_vm, OID_AUTO, phys_lookup_lists, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, sysctl_vm_phys_lookup_lists, "A", "Phys Lookup Lists");
#endif

static void _vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind,
    int domain);
static void vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind);
static int vm_phys_paddr_to_segind(vm_paddr_t pa);
static void vm_phys_split_pages(vm_page_t m, int oind, struct vm_freelist *fl,
    int order);

/*
 * Outputs the state of the physical memory allocator, specifically,
 * the amount of physical memory in each free list.
 */
static int
sysctl_vm_phys_free(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	struct vm_freelist *fl;
	int error, flind, oind, pind;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	for (flind = 0; flind < vm_nfreelists; flind++) {
		sbuf_printf(&sbuf, "\nFREE LIST %d:\n"
		    "\n  ORDER (SIZE)  |  NUMBER"
		    "\n              ", flind);
		for (pind = 0; pind < VM_NFREEPOOL; pind++)
			sbuf_printf(&sbuf, "  |  POOL %d", pind);
		sbuf_printf(&sbuf, "\n--            ");
		for (pind = 0; pind < VM_NFREEPOOL; pind++)
			sbuf_printf(&sbuf, "-- --      ");
		sbuf_printf(&sbuf, "--\n");
		for (oind = VM_NFREEORDER - 1; oind >= 0; oind--) {
			sbuf_printf(&sbuf, "  %2d (%6dK)", oind,
			    1 << (PAGE_SHIFT - 10 + oind));
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[flind][pind];
				sbuf_printf(&sbuf, "  |  %6d", fl[oind].lcnt);
			}
			sbuf_printf(&sbuf, "\n");
		}
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

/*
 * Outputs the set of physical memory segments.
 */
static int
sysctl_vm_phys_segs(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	struct vm_phys_seg *seg;
	int error, segind;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		sbuf_printf(&sbuf, "\nSEGMENT %d:\n\n", segind);
		seg = &vm_phys_segs[segind];
		sbuf_printf(&sbuf, "start:     %#jx\n",
		    (uintmax_t)seg->start);
		sbuf_printf(&sbuf, "end:       %#jx\n",
		    (uintmax_t)seg->end);
		sbuf_printf(&sbuf, "domain:    %d\n", seg->domain);
		sbuf_printf(&sbuf, "free list: %p\n", seg->free_queues);
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

#if VM_NDOMAIN > 1
/*
 * Outputs the set of free list lookup lists.
 */
static int
sysctl_vm_phys_lookup_lists(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int domain, error, flind, ndomains;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	ndomains = vm_nfreelists - VM_NFREELIST + 1;
	for (domain = 0; domain < ndomains; domain++) {
		sbuf_printf(&sbuf, "\nDOMAIN %d:\n\n", domain);
		for (flind = 0; flind < vm_nfreelists; flind++)
			sbuf_printf(&sbuf, "  [%d]:\t%p\n", flind,
			    vm_phys_lookup_lists[domain][flind]);
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}
#endif
	
/*
 * Create a physical memory segment.
 */
static void
_vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind, int domain)
{
	struct vm_phys_seg *seg;
#ifdef VM_PHYSSEG_SPARSE
	long pages;
	int segind;

	pages = 0;
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		pages += atop(seg->end - seg->start);
	}
#endif
	KASSERT(vm_phys_nsegs < VM_PHYSSEG_MAX,
	    ("vm_phys_create_seg: increase VM_PHYSSEG_MAX"));
	seg = &vm_phys_segs[vm_phys_nsegs++];
	seg->start = start;
	seg->end = end;
	seg->domain = domain;
#ifdef VM_PHYSSEG_SPARSE
	seg->first_page = &vm_page_array[pages];
#else
	seg->first_page = PHYS_TO_VM_PAGE(start);
#endif
#if VM_NDOMAIN > 1
	if (flind == VM_FREELIST_DEFAULT && domain != 0) {
		flind = VM_NFREELIST + (domain - 1);
		if (flind >= vm_nfreelists)
			vm_nfreelists = flind + 1;
	}
#endif
	seg->free_queues = &vm_phys_free_queues[flind];
}

static void
vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind)
{
	int i;

	if (mem_affinity == NULL) {
		_vm_phys_create_seg(start, end, flind, 0);
		return;
	}

	for (i = 0;; i++) {
		if (mem_affinity[i].end == 0)
			panic("Reached end of affinity info");
		if (mem_affinity[i].end <= start)
			continue;
		if (mem_affinity[i].start > start)
			panic("No affinity info for start %jx",
			    (uintmax_t)start);
		if (mem_affinity[i].end >= end) {
			_vm_phys_create_seg(start, end, flind,
			    mem_affinity[i].domain);
			break;
		}
		_vm_phys_create_seg(start, mem_affinity[i].end, flind,
		    mem_affinity[i].domain);
		start = mem_affinity[i].end;
	}
}

/*
 * Initialize the physical memory allocator.
 */
void
vm_phys_init(void)
{
	struct vm_freelist *fl;
	int flind, i, oind, pind;
#if VM_NDOMAIN > 1
	int ndomains, j;
#endif

	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
#ifdef	VM_FREELIST_ISADMA
		if (phys_avail[i] < 16777216) {
			if (phys_avail[i + 1] > 16777216) {
				vm_phys_create_seg(phys_avail[i], 16777216,
				    VM_FREELIST_ISADMA);
				vm_phys_create_seg(16777216, phys_avail[i + 1],
				    VM_FREELIST_DEFAULT);
			} else {
				vm_phys_create_seg(phys_avail[i],
				    phys_avail[i + 1], VM_FREELIST_ISADMA);
			}
			if (VM_FREELIST_ISADMA >= vm_nfreelists)
				vm_nfreelists = VM_FREELIST_ISADMA + 1;
		} else
#endif
#ifdef	VM_FREELIST_HIGHMEM
		if (phys_avail[i + 1] > VM_HIGHMEM_ADDRESS) {
			if (phys_avail[i] < VM_HIGHMEM_ADDRESS) {
				vm_phys_create_seg(phys_avail[i],
				    VM_HIGHMEM_ADDRESS, VM_FREELIST_DEFAULT);
				vm_phys_create_seg(VM_HIGHMEM_ADDRESS,
				    phys_avail[i + 1], VM_FREELIST_HIGHMEM);
			} else {
				vm_phys_create_seg(phys_avail[i],
				    phys_avail[i + 1], VM_FREELIST_HIGHMEM);
			}
			if (VM_FREELIST_HIGHMEM >= vm_nfreelists)
				vm_nfreelists = VM_FREELIST_HIGHMEM + 1;
		} else
#endif
		vm_phys_create_seg(phys_avail[i], phys_avail[i + 1],
		    VM_FREELIST_DEFAULT);
	}
	for (flind = 0; flind < vm_nfreelists; flind++) {
		for (pind = 0; pind < VM_NFREEPOOL; pind++) {
			fl = vm_phys_free_queues[flind][pind];
			for (oind = 0; oind < VM_NFREEORDER; oind++)
				TAILQ_INIT(&fl[oind].pl);
		}
	}
#if VM_NDOMAIN > 1
	/*
	 * Build a free list lookup list for each domain.  All of the
	 * memory domain lists are inserted at the VM_FREELIST_DEFAULT
	 * index in a round-robin order starting with the current
	 * domain.
	 */
	ndomains = vm_nfreelists - VM_NFREELIST + 1;
	for (flind = 0; flind < VM_FREELIST_DEFAULT; flind++)
		for (i = 0; i < ndomains; i++)
			vm_phys_lookup_lists[i][flind] =
			    &vm_phys_free_queues[flind];
	for (i = 0; i < ndomains; i++)
		for (j = 0; j < ndomains; j++) {
			flind = (i + j) % ndomains;
			if (flind == 0)
				flind = VM_FREELIST_DEFAULT;
			else
				flind += VM_NFREELIST - 1;
			vm_phys_lookup_lists[i][VM_FREELIST_DEFAULT + j] =
			    &vm_phys_free_queues[flind];
		}
	for (flind = VM_FREELIST_DEFAULT + 1; flind < VM_NFREELIST;
	     flind++)
		for (i = 0; i < ndomains; i++)
			vm_phys_lookup_lists[i][flind + ndomains - 1] =
			    &vm_phys_free_queues[flind];
#else
	for (flind = 0; flind < vm_nfreelists; flind++)
		vm_phys_lookup_lists[0][flind] = &vm_phys_free_queues[flind];
#endif

	mtx_init(&vm_phys_fictitious_reg_mtx, "vmfctr", NULL, MTX_DEF);
}

/*
 * Split a contiguous, power of two-sized set of physical pages.
 */
static __inline void
vm_phys_split_pages(vm_page_t m, int oind, struct vm_freelist *fl, int order)
{
	vm_page_t m_buddy;

	while (oind > order) {
		oind--;
		m_buddy = &m[1 << oind];
		KASSERT(m_buddy->order == VM_NFREEORDER,
		    ("vm_phys_split_pages: page %p has unexpected order %d",
		    m_buddy, m_buddy->order));
		m_buddy->order = oind;
		TAILQ_INSERT_HEAD(&fl[oind].pl, m_buddy, pageq);
		fl[oind].lcnt++;
        }
}

/*
 * Initialize a physical page and add it to the free lists.
 */
void
vm_phys_add_page(vm_paddr_t pa)
{
	vm_page_t m;

	cnt.v_page_count++;
	m = vm_phys_paddr_to_vm_page(pa);
	m->phys_addr = pa;
	m->queue = PQ_NONE;
	m->segind = vm_phys_paddr_to_segind(pa);
	m->flags = PG_FREE;
	KASSERT(m->order == VM_NFREEORDER,
	    ("vm_phys_add_page: page %p has unexpected order %d",
	    m, m->order));
	m->pool = VM_FREEPOOL_DEFAULT;
	pmap_page_init(m);
	mtx_lock(&vm_page_queue_free_mtx);
	cnt.v_free_count++;
	vm_phys_free_pages(m, 0);
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 * Allocate a contiguous, power of two-sized set of physical pages
 * from the free lists.
 *
 * The free page queues must be locked.
 */
vm_page_t
vm_phys_alloc_pages(int pool, int order)
{
	vm_page_t m;
	int flind;

	for (flind = 0; flind < vm_nfreelists; flind++) {
		m = vm_phys_alloc_freelist_pages(flind, pool, order);
		if (m != NULL)
			return (m);
	}
	return (NULL);
}

/*
 * Find and dequeue a free page on the given free list, with the 
 * specified pool and order
 */
vm_page_t
vm_phys_alloc_freelist_pages(int flind, int pool, int order)
{	
	struct vm_freelist *fl;
	struct vm_freelist *alt;
	int domain, oind, pind;
	vm_page_t m;

	KASSERT(flind < VM_NFREELIST,
	    ("vm_phys_alloc_freelist_pages: freelist %d is out of range", flind));
	KASSERT(pool < VM_NFREEPOOL,
	    ("vm_phys_alloc_freelist_pages: pool %d is out of range", pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_alloc_freelist_pages: order %d is out of range", order));

#if VM_NDOMAIN > 1
	domain = PCPU_GET(domain);
#else
	domain = 0;
#endif
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	fl = (*vm_phys_lookup_lists[domain][flind])[pool];
	for (oind = order; oind < VM_NFREEORDER; oind++) {
		m = TAILQ_FIRST(&fl[oind].pl);
		if (m != NULL) {
			TAILQ_REMOVE(&fl[oind].pl, m, pageq);
			fl[oind].lcnt--;
			m->order = VM_NFREEORDER;
			vm_phys_split_pages(m, oind, fl, order);
			return (m);
		}
	}

	/*
	 * The given pool was empty.  Find the largest
	 * contiguous, power-of-two-sized set of pages in any
	 * pool.  Transfer these pages to the given pool, and
	 * use them to satisfy the allocation.
	 */
	for (oind = VM_NFREEORDER - 1; oind >= order; oind--) {
		for (pind = 0; pind < VM_NFREEPOOL; pind++) {
			alt = (*vm_phys_lookup_lists[domain][flind])[pind];
			m = TAILQ_FIRST(&alt[oind].pl);
			if (m != NULL) {
				TAILQ_REMOVE(&alt[oind].pl, m, pageq);
				alt[oind].lcnt--;
				m->order = VM_NFREEORDER;
				vm_phys_set_pool(pool, m, oind);
				vm_phys_split_pages(m, oind, fl, order);
				return (m);
			}
		}
	}
	return (NULL);
}

/*
 * Allocate physical memory from phys_avail[].
 */
vm_paddr_t
vm_phys_bootstrap_alloc(vm_size_t size, unsigned long alignment)
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
	panic("vm_phys_bootstrap_alloc");
}

/*
 * Find the vm_page corresponding to the given physical address.
 */
vm_page_t
vm_phys_paddr_to_vm_page(vm_paddr_t pa)
{
	struct vm_phys_seg *seg;
	int segind;

	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		if (pa >= seg->start && pa < seg->end)
			return (&seg->first_page[atop(pa - seg->start)]);
	}
	return (NULL);
}

vm_page_t
vm_phys_fictitious_to_vm_page(vm_paddr_t pa)
{
	struct vm_phys_fictitious_seg *seg;
	vm_page_t m;
	int segind;

	m = NULL;
	for (segind = 0; segind < VM_PHYS_FICTITIOUS_NSEGS; segind++) {
		seg = &vm_phys_fictitious_segs[segind];
		if (pa >= seg->start && pa < seg->end) {
			m = &seg->first_page[atop(pa - seg->start)];
			KASSERT((m->flags & PG_FICTITIOUS) != 0,
			    ("%p not fictitious", m));
			break;
		}
	}
	return (m);
}

int
vm_phys_fictitious_reg_range(vm_paddr_t start, vm_paddr_t end,
    vm_memattr_t memattr)
{
	struct vm_phys_fictitious_seg *seg;
	vm_page_t fp;
	long i, page_count;
	int segind;
#ifdef VM_PHYSSEG_DENSE
	long pi;
	boolean_t malloced;
#endif

	page_count = (end - start) / PAGE_SIZE;

#ifdef VM_PHYSSEG_DENSE
	pi = atop(start);
	if (pi >= first_page && atop(end) < vm_page_array_size) {
		fp = &vm_page_array[pi - first_page];
		malloced = FALSE;
	} else
#endif
	{
		fp = malloc(page_count * sizeof(struct vm_page), M_FICT_PAGES,
		    M_WAITOK | M_ZERO);
#ifdef VM_PHYSSEG_DENSE
		malloced = TRUE;
#endif
	}
	for (i = 0; i < page_count; i++) {
		vm_page_initfake(&fp[i], start + PAGE_SIZE * i, memattr);
		pmap_page_init(&fp[i]);
		fp[i].oflags &= ~(VPO_BUSY | VPO_UNMANAGED);
	}
	mtx_lock(&vm_phys_fictitious_reg_mtx);
	for (segind = 0; segind < VM_PHYS_FICTITIOUS_NSEGS; segind++) {
		seg = &vm_phys_fictitious_segs[segind];
		if (seg->start == 0 && seg->end == 0) {
			seg->start = start;
			seg->end = end;
			seg->first_page = fp;
			mtx_unlock(&vm_phys_fictitious_reg_mtx);
			return (0);
		}
	}
	mtx_unlock(&vm_phys_fictitious_reg_mtx);
#ifdef VM_PHYSSEG_DENSE
	if (malloced)
#endif
		free(fp, M_FICT_PAGES);
	return (EBUSY);
}

void
vm_phys_fictitious_unreg_range(vm_paddr_t start, vm_paddr_t end)
{
	struct vm_phys_fictitious_seg *seg;
	vm_page_t fp;
	int segind;
#ifdef VM_PHYSSEG_DENSE
	long pi;
#endif

#ifdef VM_PHYSSEG_DENSE
	pi = atop(start);
#endif

	mtx_lock(&vm_phys_fictitious_reg_mtx);
	for (segind = 0; segind < VM_PHYS_FICTITIOUS_NSEGS; segind++) {
		seg = &vm_phys_fictitious_segs[segind];
		if (seg->start == start && seg->end == end) {
			seg->start = seg->end = 0;
			fp = seg->first_page;
			seg->first_page = NULL;
			mtx_unlock(&vm_phys_fictitious_reg_mtx);
#ifdef VM_PHYSSEG_DENSE
			if (pi < first_page || atop(end) >= vm_page_array_size)
#endif
				free(fp, M_FICT_PAGES);
			return;
		}
	}
	mtx_unlock(&vm_phys_fictitious_reg_mtx);
	KASSERT(0, ("Unregistering not registered fictitious range"));
}

/*
 * Find the segment containing the given physical address.
 */
static int
vm_phys_paddr_to_segind(vm_paddr_t pa)
{
	struct vm_phys_seg *seg;
	int segind;

	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		if (pa >= seg->start && pa < seg->end)
			return (segind);
	}
	panic("vm_phys_paddr_to_segind: paddr %#jx is not in any segment" ,
	    (uintmax_t)pa);
}

/*
 * Free a contiguous, power of two-sized set of physical pages.
 *
 * The free page queues must be locked.
 */
void
vm_phys_free_pages(vm_page_t m, int order)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa, pa_buddy;
	vm_page_t m_buddy;

	KASSERT(m->order == VM_NFREEORDER,
	    ("vm_phys_free_pages: page %p has unexpected order %d",
	    m, m->order));
	KASSERT(m->pool < VM_NFREEPOOL,
	    ("vm_phys_free_pages: page %p has unexpected pool %d",
	    m, m->pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_free_pages: order %d is out of range", order));
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	pa = VM_PAGE_TO_PHYS(m);
	seg = &vm_phys_segs[m->segind];
	while (order < VM_NFREEORDER - 1) {
		pa_buddy = pa ^ (1 << (PAGE_SHIFT + order));
		if (pa_buddy < seg->start ||
		    pa_buddy >= seg->end)
			break;
		m_buddy = &seg->first_page[atop(pa_buddy - seg->start)];
		if (m_buddy->order != order)
			break;
		fl = (*seg->free_queues)[m_buddy->pool];
		TAILQ_REMOVE(&fl[m_buddy->order].pl, m_buddy, pageq);
		fl[m_buddy->order].lcnt--;
		m_buddy->order = VM_NFREEORDER;
		if (m_buddy->pool != m->pool)
			vm_phys_set_pool(m->pool, m_buddy, order);
		order++;
		pa &= ~((1 << (PAGE_SHIFT + order)) - 1);
		m = &seg->first_page[atop(pa - seg->start)];
	}
	m->order = order;
	fl = (*seg->free_queues)[m->pool];
	TAILQ_INSERT_TAIL(&fl[order].pl, m, pageq);
	fl[order].lcnt++;
}

/*
 * Set the pool for a contiguous, power of two-sized set of physical pages. 
 */
void
vm_phys_set_pool(int pool, vm_page_t m, int order)
{
	vm_page_t m_tmp;

	for (m_tmp = m; m_tmp < &m[1 << order]; m_tmp++)
		m_tmp->pool = pool;
}

/*
 * Search for the given physical page "m" in the free lists.  If the search
 * succeeds, remove "m" from the free lists and return TRUE.  Otherwise, return
 * FALSE, indicating that "m" is not in the free lists.
 *
 * The free page queues must be locked.
 */
boolean_t
vm_phys_unfree_page(vm_page_t m)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa, pa_half;
	vm_page_t m_set, m_tmp;
	int order;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);

	/*
	 * First, find the contiguous, power of two-sized set of free
	 * physical pages containing the given physical page "m" and
	 * assign it to "m_set".
	 */
	seg = &vm_phys_segs[m->segind];
	for (m_set = m, order = 0; m_set->order == VM_NFREEORDER &&
	    order < VM_NFREEORDER - 1; ) {
		order++;
		pa = m->phys_addr & (~(vm_paddr_t)0 << (PAGE_SHIFT + order));
		if (pa >= seg->start)
			m_set = &seg->first_page[atop(pa - seg->start)];
		else
			return (FALSE);
	}
	if (m_set->order < order)
		return (FALSE);
	if (m_set->order == VM_NFREEORDER)
		return (FALSE);
	KASSERT(m_set->order < VM_NFREEORDER,
	    ("vm_phys_unfree_page: page %p has unexpected order %d",
	    m_set, m_set->order));

	/*
	 * Next, remove "m_set" from the free lists.  Finally, extract
	 * "m" from "m_set" using an iterative algorithm: While "m_set"
	 * is larger than a page, shrink "m_set" by returning the half
	 * of "m_set" that does not contain "m" to the free lists.
	 */
	fl = (*seg->free_queues)[m_set->pool];
	order = m_set->order;
	TAILQ_REMOVE(&fl[order].pl, m_set, pageq);
	fl[order].lcnt--;
	m_set->order = VM_NFREEORDER;
	while (order > 0) {
		order--;
		pa_half = m_set->phys_addr ^ (1 << (PAGE_SHIFT + order));
		if (m->phys_addr < pa_half)
			m_tmp = &seg->first_page[atop(pa_half - seg->start)];
		else {
			m_tmp = m_set;
			m_set = &seg->first_page[atop(pa_half - seg->start)];
		}
		m_tmp->order = order;
		TAILQ_INSERT_HEAD(&fl[order].pl, m_tmp, pageq);
		fl[order].lcnt++;
	}
	KASSERT(m_set == m, ("vm_phys_unfree_page: fatal inconsistency"));
	return (TRUE);
}

/*
 * Try to zero one physical page.  Used by an idle priority thread.
 */
boolean_t
vm_phys_zero_pages_idle(void)
{
	static struct vm_freelist *fl = vm_phys_free_queues[0][0];
	static int flind, oind, pind;
	vm_page_t m, m_tmp;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	for (;;) {
		TAILQ_FOREACH_REVERSE(m, &fl[oind].pl, pglist, pageq) {
			for (m_tmp = m; m_tmp < &m[1 << oind]; m_tmp++) {
				if ((m_tmp->flags & (PG_CACHED | PG_ZERO)) == 0) {
					vm_phys_unfree_page(m_tmp);
					cnt.v_free_count--;
					mtx_unlock(&vm_page_queue_free_mtx);
					pmap_zero_page_idle(m_tmp);
					m_tmp->flags |= PG_ZERO;
					mtx_lock(&vm_page_queue_free_mtx);
					cnt.v_free_count++;
					vm_phys_free_pages(m_tmp, 0);
					vm_page_zero_count++;
					cnt_prezero++;
					return (TRUE);
				}
			}
		}
		oind++;
		if (oind == VM_NFREEORDER) {
			oind = 0;
			pind++;
			if (pind == VM_NFREEPOOL) {
				pind = 0;
				flind++;
				if (flind == vm_nfreelists)
					flind = 0;
			}
			fl = vm_phys_free_queues[flind][pind];
		}
	}
}

/*
 * Allocate a contiguous set of physical pages of the given size
 * "npages" from the free lists.  All of the physical pages must be at
 * or above the given physical address "low" and below the given
 * physical address "high".  The given value "alignment" determines the
 * alignment of the first physical page in the set.  If the given value
 * "boundary" is non-zero, then the set of physical pages cannot cross
 * any physical address boundary that is a multiple of that value.  Both
 * "alignment" and "boundary" must be a power of two.
 */
vm_page_t
vm_phys_alloc_contig(unsigned long npages, vm_paddr_t low, vm_paddr_t high,
    unsigned long alignment, unsigned long boundary)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	struct vnode *vp;
	vm_paddr_t pa, pa_last, size;
	vm_page_t deferred_vdrop_list, m, m_ret;
	int domain, flind, i, oind, order, pind;

#if VM_NDOMAIN > 1
	domain = PCPU_GET(domain);
#else
	domain = 0;
#endif
	size = npages << PAGE_SHIFT;
	KASSERT(size != 0,
	    ("vm_phys_alloc_contig: size must not be 0"));
	KASSERT((alignment & (alignment - 1)) == 0,
	    ("vm_phys_alloc_contig: alignment must be a power of 2"));
	KASSERT((boundary & (boundary - 1)) == 0,
	    ("vm_phys_alloc_contig: boundary must be a power of 2"));
	deferred_vdrop_list = NULL;
	/* Compute the queue that is the best fit for npages. */
	for (order = 0; (1 << order) < npages; order++);
	mtx_lock(&vm_page_queue_free_mtx);
#if VM_NRESERVLEVEL > 0
retry:
#endif
	for (flind = 0; flind < vm_nfreelists; flind++) {
		for (oind = min(order, VM_NFREEORDER - 1); oind < VM_NFREEORDER; oind++) {
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = (*vm_phys_lookup_lists[domain][flind])
				    [pind];
				TAILQ_FOREACH(m_ret, &fl[oind].pl, pageq) {
					/*
					 * A free list may contain physical pages
					 * from one or more segments.
					 */
					seg = &vm_phys_segs[m_ret->segind];
					if (seg->start > high ||
					    low >= seg->end)
						continue;

					/*
					 * Is the size of this allocation request
					 * larger than the largest block size?
					 */
					if (order >= VM_NFREEORDER) {
						/*
						 * Determine if a sufficient number
						 * of subsequent blocks to satisfy
						 * the allocation request are free.
						 */
						pa = VM_PAGE_TO_PHYS(m_ret);
						pa_last = pa + size;
						for (;;) {
							pa += 1 << (PAGE_SHIFT + VM_NFREEORDER - 1);
							if (pa >= pa_last)
								break;
							if (pa < seg->start ||
							    pa >= seg->end)
								break;
							m = &seg->first_page[atop(pa - seg->start)];
							if (m->order != VM_NFREEORDER - 1)
								break;
						}
						/* If not, continue to the next block. */
						if (pa < pa_last)
							continue;
					}

					/*
					 * Determine if the blocks are within the given range,
					 * satisfy the given alignment, and do not cross the
					 * given boundary.
					 */
					pa = VM_PAGE_TO_PHYS(m_ret);
					if (pa >= low &&
					    pa + size <= high &&
					    (pa & (alignment - 1)) == 0 &&
					    ((pa ^ (pa + size - 1)) & ~(boundary - 1)) == 0)
						goto done;
				}
			}
		}
	}
#if VM_NRESERVLEVEL > 0
	if (vm_reserv_reclaim_contig(size, low, high, alignment, boundary))
		goto retry;
#endif
	mtx_unlock(&vm_page_queue_free_mtx);
	return (NULL);
done:
	for (m = m_ret; m < &m_ret[npages]; m = &m[1 << oind]) {
		fl = (*seg->free_queues)[m->pool];
		TAILQ_REMOVE(&fl[m->order].pl, m, pageq);
		fl[m->order].lcnt--;
		m->order = VM_NFREEORDER;
	}
	if (m_ret->pool != VM_FREEPOOL_DEFAULT)
		vm_phys_set_pool(VM_FREEPOOL_DEFAULT, m_ret, oind);
	fl = (*seg->free_queues)[m_ret->pool];
	vm_phys_split_pages(m_ret, oind, fl, order);
	for (i = 0; i < npages; i++) {
		m = &m_ret[i];
		vp = vm_page_alloc_init(m);
		if (vp != NULL) {
			/*
			 * Enqueue the vnode for deferred vdrop().
			 *
			 * Unmanaged pages don't use "pageq", so it
			 * can be safely abused to construct a short-
			 * lived queue of vnodes.
			 */
			m->pageq.tqe_prev = (void *)vp;
			m->pageq.tqe_next = deferred_vdrop_list;
			deferred_vdrop_list = m;
		}
	}
	for (; i < roundup2(npages, 1 << imin(oind, order)); i++) {
		m = &m_ret[i];
		KASSERT(m->order == VM_NFREEORDER,
		    ("vm_phys_alloc_contig: page %p has unexpected order %d",
		    m, m->order));
		vm_phys_free_pages(m, 0);
	}
	mtx_unlock(&vm_page_queue_free_mtx);
	while (deferred_vdrop_list != NULL) {
		vdrop((struct vnode *)deferred_vdrop_list->pageq.tqe_prev);
		deferred_vdrop_list = deferred_vdrop_list->pageq.tqe_next;
	}
	return (m_ret);
}

#ifdef DDB
/*
 * Show the number of physical pages in each of the free lists.
 */
DB_SHOW_COMMAND(freepages, db_show_freepages)
{
	struct vm_freelist *fl;
	int flind, oind, pind;

	for (flind = 0; flind < vm_nfreelists; flind++) {
		db_printf("FREE LIST %d:\n"
		    "\n  ORDER (SIZE)  |  NUMBER"
		    "\n              ", flind);
		for (pind = 0; pind < VM_NFREEPOOL; pind++)
			db_printf("  |  POOL %d", pind);
		db_printf("\n--            ");
		for (pind = 0; pind < VM_NFREEPOOL; pind++)
			db_printf("-- --      ");
		db_printf("--\n");
		for (oind = VM_NFREEORDER - 1; oind >= 0; oind--) {
			db_printf("  %2.2d (%6.6dK)", oind,
			    1 << (PAGE_SHIFT - 10 + oind));
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[flind][pind];
				db_printf("  |  %6.6d", fl[oind].lcnt);
			}
			db_printf("\n");
		}
		db_printf("\n");
	}
}
#endif
