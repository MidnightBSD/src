/******************************************************************************
 * gnttab.c
 * 
 * Two sets of functionality:
 * 1. Granting foreign access to our memory reservation.
 * 2. Accessing others' memory reservations via grant references.
 * (i.e., mechanisms for both sender and recipient of grant references)
 * 
 * Copyright (c) 2005, Christopher Clark
 * Copyright (c) 2004, K A Fraser
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_global.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>

#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>
#include <machine/xen/synch_bitops.h>

#include <xen/hypervisor.h>
#include <xen/gnttab.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#define cmpxchg(a, b, c) atomic_cmpset_int((volatile u_int *)(a),(b),(c))

/* External tools reserve first few grant table entries. */
#define NR_RESERVED_ENTRIES 8
#define GREFS_PER_GRANT_FRAME (PAGE_SIZE / sizeof(grant_entry_t))

static grant_ref_t **gnttab_list;
static unsigned int nr_grant_frames;
static unsigned int boot_max_nr_grant_frames;
static int gnttab_free_count;
static grant_ref_t gnttab_free_head;
static struct mtx gnttab_list_lock;

static grant_entry_t *shared;

static struct gnttab_free_callback *gnttab_free_callback_list = NULL;

static int gnttab_expand(unsigned int req_entries);

#define RPP (PAGE_SIZE / sizeof(grant_ref_t))
#define gnttab_entry(entry) (gnttab_list[(entry) / RPP][(entry) % RPP])

static int
get_free_entries(int count, int *entries)
{
	int ref, error;
	grant_ref_t head;

	mtx_lock(&gnttab_list_lock);
	if ((gnttab_free_count < count) &&
	    ((error = gnttab_expand(count - gnttab_free_count)) != 0)) {
		mtx_unlock(&gnttab_list_lock);
		return (error);
	}
	ref = head = gnttab_free_head;
	gnttab_free_count -= count;
	while (count-- > 1)
		head = gnttab_entry(head);
	gnttab_free_head = gnttab_entry(head);
	gnttab_entry(head) = GNTTAB_LIST_END;
	mtx_unlock(&gnttab_list_lock);

	*entries = ref;
	return (0);
}

static void
do_free_callbacks(void)
{
	struct gnttab_free_callback *callback, *next;

	callback = gnttab_free_callback_list;
	gnttab_free_callback_list = NULL;

	while (callback != NULL) {
		next = callback->next;
		if (gnttab_free_count >= callback->count) {
			callback->next = NULL;
			callback->fn(callback->arg);
		} else {
			callback->next = gnttab_free_callback_list;
			gnttab_free_callback_list = callback;
		}
		callback = next;
	}
}

static inline void
check_free_callbacks(void)
{
	if (unlikely(gnttab_free_callback_list != NULL))
		do_free_callbacks();
}

static void
put_free_entry(grant_ref_t ref)
{

	mtx_lock(&gnttab_list_lock);
	gnttab_entry(ref) = gnttab_free_head;
	gnttab_free_head = ref;
	gnttab_free_count++;
	check_free_callbacks();
	mtx_unlock(&gnttab_list_lock);
}

/*
 * Public grant-issuing interface functions
 */

int
gnttab_grant_foreign_access(domid_t domid, unsigned long frame, int readonly,
	grant_ref_t *result)
{
	int error, ref;

	error = get_free_entries(1, &ref);

	if (unlikely(error))
		return (error);

	shared[ref].frame = frame;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);

	if (result)
		*result = ref;

	return (0);
}

void
gnttab_grant_foreign_access_ref(grant_ref_t ref, domid_t domid,
				unsigned long frame, int readonly)
{

	shared[ref].frame = frame;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);
}

int
gnttab_query_foreign_access(grant_ref_t ref)
{
	uint16_t nflags;

	nflags = shared[ref].flags;

	return (nflags & (GTF_reading|GTF_writing));
}

int
gnttab_end_foreign_access_ref(grant_ref_t ref)
{
	uint16_t flags, nflags;

	nflags = shared[ref].flags;
	do {
		if ( (flags = nflags) & (GTF_reading|GTF_writing) ) {
			printf("%s: WARNING: g.e. still in use!\n", __func__);
			return (0);
		}
	} while ((nflags = synch_cmpxchg(&shared[ref].flags, flags, 0)) !=
	       flags);

	return (1);
}

void
gnttab_end_foreign_access(grant_ref_t ref, void *page)
{
	if (gnttab_end_foreign_access_ref(ref)) {
		put_free_entry(ref);
		if (page != NULL) {
			free(page, M_DEVBUF);
		}
	}
	else {
		/* XXX This needs to be fixed so that the ref and page are
		   placed on a list to be freed up later. */
		printf("%s: WARNING: leaking g.e. and page still in use!\n",
		       __func__);
	}
}

void
gnttab_end_foreign_access_references(u_int count, grant_ref_t *refs)
{
	grant_ref_t *last_ref;
	grant_ref_t  head;
	grant_ref_t  tail;

	head = GNTTAB_LIST_END;
	tail = *refs;
	last_ref = refs + count;
	while (refs != last_ref) {

		if (gnttab_end_foreign_access_ref(*refs)) {
			gnttab_entry(*refs) = head;
			head = *refs;
		} else {
			/*
			 * XXX This needs to be fixed so that the ref 
			 * is placed on a list to be freed up later.
			 */
			printf("%s: WARNING: leaking g.e. still in use!\n",
			       __func__);
			count--;
		}
		refs++;
	}

	if (count != 0) {
		mtx_lock(&gnttab_list_lock);
		gnttab_free_count += count;
		gnttab_entry(tail) = gnttab_free_head;
		gnttab_free_head = head;
		mtx_unlock(&gnttab_list_lock);
	}
}

int
gnttab_grant_foreign_transfer(domid_t domid, unsigned long pfn,
    grant_ref_t *result)
{
	int error, ref;

	error = get_free_entries(1, &ref);
	if (unlikely(error))
		return (error);

	gnttab_grant_foreign_transfer_ref(ref, domid, pfn);

	*result = ref;
	return (0);
}

void
gnttab_grant_foreign_transfer_ref(grant_ref_t ref, domid_t domid,
	unsigned long pfn)
{
	shared[ref].frame = pfn;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_accept_transfer;
}

unsigned long
gnttab_end_foreign_transfer_ref(grant_ref_t ref)
{
	unsigned long frame;
	uint16_t      flags;

	/*
         * If a transfer is not even yet started, try to reclaim the grant
         * reference and return failure (== 0).
         */
	while (!((flags = shared[ref].flags) & GTF_transfer_committed)) {
		if ( synch_cmpxchg(&shared[ref].flags, flags, 0) == flags )
			return (0);
		cpu_relax();
	}

	/* If a transfer is in progress then wait until it is completed. */
	while (!(flags & GTF_transfer_completed)) {
		flags = shared[ref].flags;
		cpu_relax();
	}

	/* Read the frame number /after/ reading completion status. */
	rmb();
	frame = shared[ref].frame;
	KASSERT(frame != 0, ("grant table inconsistent"));

	return (frame);
}

unsigned long
gnttab_end_foreign_transfer(grant_ref_t ref)
{
	unsigned long frame = gnttab_end_foreign_transfer_ref(ref);

	put_free_entry(ref);
	return (frame);
}

void
gnttab_free_grant_reference(grant_ref_t ref)
{

	put_free_entry(ref);
}

void
gnttab_free_grant_references(grant_ref_t head)
{
	grant_ref_t ref;
	int count = 1;

	if (head == GNTTAB_LIST_END)
		return;

	ref = head;
	while (gnttab_entry(ref) != GNTTAB_LIST_END) {
		ref = gnttab_entry(ref);
		count++;
	}
	mtx_lock(&gnttab_list_lock);
	gnttab_entry(ref) = gnttab_free_head;
	gnttab_free_head = head;
	gnttab_free_count += count;
	check_free_callbacks();
	mtx_unlock(&gnttab_list_lock);
}

int
gnttab_alloc_grant_references(uint16_t count, grant_ref_t *head)
{
	int ref, error;

	error = get_free_entries(count, &ref);
	if (unlikely(error))
		return (error);

	*head = ref;
	return (0);
}

int
gnttab_empty_grant_references(const grant_ref_t *private_head)
{

	return (*private_head == GNTTAB_LIST_END);
}

int
gnttab_claim_grant_reference(grant_ref_t *private_head)
{
	grant_ref_t g = *private_head;

	if (unlikely(g == GNTTAB_LIST_END))
		return (g);
	*private_head = gnttab_entry(g);
	return (g);
}

void
gnttab_release_grant_reference(grant_ref_t *private_head, grant_ref_t  release)
{

	gnttab_entry(release) = *private_head;
	*private_head = release;
}

void
gnttab_request_free_callback(struct gnttab_free_callback *callback,
    void (*fn)(void *), void *arg, uint16_t count)
{

	mtx_lock(&gnttab_list_lock);
	if (callback->next)
		goto out;
	callback->fn = fn;
	callback->arg = arg;
	callback->count = count;
	callback->next = gnttab_free_callback_list;
	gnttab_free_callback_list = callback;
	check_free_callbacks();
 out:
	mtx_unlock(&gnttab_list_lock);

}

void
gnttab_cancel_free_callback(struct gnttab_free_callback *callback)
{
	struct gnttab_free_callback **pcb;

	mtx_lock(&gnttab_list_lock);
	for (pcb = &gnttab_free_callback_list; *pcb; pcb = &(*pcb)->next) {
		if (*pcb == callback) {
			*pcb = callback->next;
			break;
		}
	}
	mtx_unlock(&gnttab_list_lock);
}


static int
grow_gnttab_list(unsigned int more_frames)
{
	unsigned int new_nr_grant_frames, extra_entries, i;

	new_nr_grant_frames = nr_grant_frames + more_frames;
	extra_entries       = more_frames * GREFS_PER_GRANT_FRAME;

	for (i = nr_grant_frames; i < new_nr_grant_frames; i++)
	{
		gnttab_list[i] = (grant_ref_t *)
			malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);

		if (!gnttab_list[i])
			goto grow_nomem;
	}

	for (i = GREFS_PER_GRANT_FRAME * nr_grant_frames;
	     i < GREFS_PER_GRANT_FRAME * new_nr_grant_frames - 1; i++)
		gnttab_entry(i) = i + 1;

	gnttab_entry(i) = gnttab_free_head;
	gnttab_free_head = GREFS_PER_GRANT_FRAME * nr_grant_frames;
	gnttab_free_count += extra_entries;

	nr_grant_frames = new_nr_grant_frames;

	check_free_callbacks();

	return (0);

grow_nomem:
	for ( ; i >= nr_grant_frames; i--)
		free(gnttab_list[i], M_DEVBUF);
	return (ENOMEM);
}

static unsigned int
__max_nr_grant_frames(void)
{
	struct gnttab_query_size query;
	int rc;

	query.dom = DOMID_SELF;

	rc = HYPERVISOR_grant_table_op(GNTTABOP_query_size, &query, 1);
	if ((rc < 0) || (query.status != GNTST_okay))
		return (4); /* Legacy max supported number of frames */

	return (query.max_nr_frames);
}

static inline
unsigned int max_nr_grant_frames(void)
{
	unsigned int xen_max = __max_nr_grant_frames();

	if (xen_max > boot_max_nr_grant_frames)
		return (boot_max_nr_grant_frames);
	return (xen_max);
}

#ifdef notyet
/*
 * XXX needed for backend support
 *
 */
static int
map_pte_fn(pte_t *pte, struct page *pmd_page,
		      unsigned long addr, void *data)
{
	unsigned long **frames = (unsigned long **)data;

	set_pte_at(&init_mm, addr, pte, pfn_pte_ma((*frames)[0], PAGE_KERNEL));
	(*frames)++;
	return 0;
}

static int
unmap_pte_fn(pte_t *pte, struct page *pmd_page,
			unsigned long addr, void *data)
{

	set_pte_at(&init_mm, addr, pte, __pte(0));
	return 0;
}
#endif

#ifndef XENHVM

static int
gnttab_map(unsigned int start_idx, unsigned int end_idx)
{
	struct gnttab_setup_table setup;
	u_long *frames;

	unsigned int nr_gframes = end_idx + 1;
	int i, rc;

	frames = malloc(nr_gframes * sizeof(unsigned long), M_DEVBUF, M_NOWAIT);
	if (!frames)
		return (ENOMEM);

	setup.dom        = DOMID_SELF;
	setup.nr_frames  = nr_gframes;
	set_xen_guest_handle(setup.frame_list, frames);

	rc = HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
	if (rc == -ENOSYS) {
		free(frames, M_DEVBUF);
		return (ENOSYS);
	}
	KASSERT(!(rc || setup.status),
	    ("unexpected result from grant_table_op"));

	if (shared == NULL) {
		vm_offset_t area;

		area = kmem_alloc_nofault(kernel_map,
		    PAGE_SIZE * max_nr_grant_frames());
		KASSERT(area, ("can't allocate VM space for grant table"));
		shared = (grant_entry_t *)area;
	}

	for (i = 0; i < nr_gframes; i++)
		PT_SET_MA(((caddr_t)shared) + i*PAGE_SIZE, 
		    ((vm_paddr_t)frames[i]) << PAGE_SHIFT | PG_RW | PG_V);

	free(frames, M_DEVBUF);

	return (0);
}

int
gnttab_resume(void)
{

	if (max_nr_grant_frames() < nr_grant_frames)
		return (ENOSYS);
	return (gnttab_map(0, nr_grant_frames - 1));
}

int
gnttab_suspend(void)
{
	int i;

	for (i = 0; i < nr_grant_frames; i++)
		pmap_kremove((vm_offset_t) shared + i * PAGE_SIZE);

	return (0);
}

#else /* XENHVM */

#include <dev/xen/xenpci/xenpcivar.h>

static vm_paddr_t resume_frames;

static int
gnttab_map(unsigned int start_idx, unsigned int end_idx)
{
	struct xen_add_to_physmap xatp;
	unsigned int i = end_idx;

	/*
	 * Loop backwards, so that the first hypercall has the largest index,
	 * ensuring that the table will grow only once.
	 */
	do {
		xatp.domid = DOMID_SELF;
		xatp.idx = i;
		xatp.space = XENMAPSPACE_grant_table;
		xatp.gpfn = (resume_frames >> PAGE_SHIFT) + i;
		if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
			panic("HYPERVISOR_memory_op failed to map gnttab");
	} while (i-- > start_idx);

	if (shared == NULL) {
		vm_offset_t area;

		area = kmem_alloc_nofault(kernel_map,
		    PAGE_SIZE * max_nr_grant_frames());
		KASSERT(area, ("can't allocate VM space for grant table"));
		shared = (grant_entry_t *)area;
	}

	for (i = start_idx; i <= end_idx; i++) {
		pmap_kenter((vm_offset_t) shared + i * PAGE_SIZE,
		    resume_frames + i * PAGE_SIZE);
	}

	return (0);
}

int
gnttab_resume(void)
{
	int error;
	unsigned int max_nr_gframes, nr_gframes;

	nr_gframes = nr_grant_frames;
	max_nr_gframes = max_nr_grant_frames();
	if (max_nr_gframes < nr_gframes)
		return (ENOSYS);

	if (!resume_frames) {
		error = xenpci_alloc_space(PAGE_SIZE * max_nr_gframes,
		    &resume_frames);
		if (error) {
			printf("error mapping gnttab share frames\n");
			return (error);
		}
	}

	return (gnttab_map(0, nr_gframes - 1));
}

#endif

static int
gnttab_expand(unsigned int req_entries)
{
	int error;
	unsigned int cur, extra;

	cur = nr_grant_frames;
	extra = ((req_entries + (GREFS_PER_GRANT_FRAME-1)) /
		 GREFS_PER_GRANT_FRAME);
	if (cur + extra > max_nr_grant_frames())
		return (ENOSPC);

	error = gnttab_map(cur, cur + extra - 1);
	if (!error)
		error = grow_gnttab_list(extra);

	return (error);
}

int 
gnttab_init()
{
	int i;
	unsigned int max_nr_glist_frames;
	unsigned int nr_init_grefs;

	if (!is_running_on_xen())
		return (ENODEV);

	nr_grant_frames = 1;
	boot_max_nr_grant_frames = __max_nr_grant_frames();

	/* Determine the maximum number of frames required for the
	 * grant reference free list on the current hypervisor.
	 */
	max_nr_glist_frames = (boot_max_nr_grant_frames *
			       GREFS_PER_GRANT_FRAME /
			       (PAGE_SIZE / sizeof(grant_ref_t)));

	gnttab_list = malloc(max_nr_glist_frames * sizeof(grant_ref_t *),
	    M_DEVBUF, M_NOWAIT);

	if (gnttab_list == NULL)
		return (ENOMEM);

	for (i = 0; i < nr_grant_frames; i++) {
		gnttab_list[i] = (grant_ref_t *)
			malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
		if (gnttab_list[i] == NULL)
			goto ini_nomem;
	}

	if (gnttab_resume())
		return (ENODEV);

	nr_init_grefs = nr_grant_frames * GREFS_PER_GRANT_FRAME;

	for (i = NR_RESERVED_ENTRIES; i < nr_init_grefs - 1; i++)
		gnttab_entry(i) = i + 1;

	gnttab_entry(nr_init_grefs - 1) = GNTTAB_LIST_END;
	gnttab_free_count = nr_init_grefs - NR_RESERVED_ENTRIES;
	gnttab_free_head  = NR_RESERVED_ENTRIES;

	if (bootverbose)
		printf("Grant table initialized\n");

	return (0);

ini_nomem:
	for (i--; i >= 0; i--)
		free(gnttab_list[i], M_DEVBUF);
	free(gnttab_list, M_DEVBUF);
	return (ENOMEM);

}

MTX_SYSINIT(gnttab, &gnttab_list_lock, "GNTTAB LOCK", MTX_DEF); 
