/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)vm_object.c	8.5 (Berkeley) 3/22/94
 *
 *
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
 *	Virtual memory object module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/vm/vm_object.c 172789 2007-10-19 05:48:45Z alc $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <sys/proc.h>		/* for curproc, pageproc */
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#define EASY_SCAN_FACTOR       8

#define MSYNC_FLUSH_HARDSEQ	0x01
#define MSYNC_FLUSH_SOFTSEQ	0x02

/*
 * msync / VM object flushing optimizations
 */
static int msync_flush_flags = MSYNC_FLUSH_HARDSEQ | MSYNC_FLUSH_SOFTSEQ;
SYSCTL_INT(_vm, OID_AUTO, msync_flush_flags,
        CTLFLAG_RW, &msync_flush_flags, 0, "");

static int old_msync;
SYSCTL_INT(_vm, OID_AUTO, old_msync, CTLFLAG_RW, &old_msync, 0,
    "Use old (insecure) msync behavior");

static void	vm_object_qcollapse(vm_object_t object);
static int	vm_object_page_collect_flush(vm_object_t object, vm_page_t p, int curgeneration, int pagerflags);
static void	vm_object_vndeallocate(vm_object_t object);

/*
 *	Virtual memory objects maintain the actual data
 *	associated with allocated virtual memory.  A given
 *	page of memory exists within exactly one object.
 *
 *	An object is only deallocated when all "references"
 *	are given up.  Only one "reference" to a given
 *	region of an object should be writeable.
 *
 *	Associated with each object is a list of all resident
 *	memory pages belonging to that object; this list is
 *	maintained by the "vm_page" module, and locked by the object's
 *	lock.
 *
 *	Each object also records a "pager" routine which is
 *	used to retrieve (and store) pages to the proper backing
 *	storage.  In addition, objects may be backed by other
 *	objects from which they were virtual-copied.
 *
 *	The only items within the object structure which are
 *	modified after time of creation are:
 *		reference count		locked by object's lock
 *		pager routine		locked by object's lock
 *
 */

struct object_q vm_object_list;
struct mtx vm_object_list_mtx;	/* lock for object list and count */

struct vm_object kernel_object_store;
struct vm_object kmem_object_store;

SYSCTL_NODE(_vm_stats, OID_AUTO, object, CTLFLAG_RD, 0, "VM object stats");

static long object_collapses;
SYSCTL_LONG(_vm_stats_object, OID_AUTO, collapses, CTLFLAG_RD,
    &object_collapses, 0, "VM object collapses");

static long object_bypasses;
SYSCTL_LONG(_vm_stats_object, OID_AUTO, bypasses, CTLFLAG_RD,
    &object_bypasses, 0, "VM object bypasses");

static uma_zone_t obj_zone;

static int vm_object_zinit(void *mem, int size, int flags);

#ifdef INVARIANTS
static void vm_object_zdtor(void *mem, int size, void *arg);

static void
vm_object_zdtor(void *mem, int size, void *arg)
{
	vm_object_t object;

	object = (vm_object_t)mem;
	KASSERT(TAILQ_EMPTY(&object->memq),
	    ("object %p has resident pages",
	    object));
	KASSERT(object->cache == NULL,
	    ("object %p has cached pages",
	    object));
	KASSERT(object->paging_in_progress == 0,
	    ("object %p paging_in_progress = %d",
	    object, object->paging_in_progress));
	KASSERT(object->resident_page_count == 0,
	    ("object %p resident_page_count = %d",
	    object, object->resident_page_count));
	KASSERT(object->shadow_count == 0,
	    ("object %p shadow_count = %d",
	    object, object->shadow_count));
}
#endif

static int
vm_object_zinit(void *mem, int size, int flags)
{
	vm_object_t object;

	object = (vm_object_t)mem;
	bzero(&object->mtx, sizeof(object->mtx));
	VM_OBJECT_LOCK_INIT(object, "standard object");

	/* These are true for any object that has been freed */
	object->paging_in_progress = 0;
	object->resident_page_count = 0;
	object->shadow_count = 0;
	return (0);
}

void
_vm_object_allocate(objtype_t type, vm_pindex_t size, vm_object_t object)
{

	TAILQ_INIT(&object->memq);
	LIST_INIT(&object->shadow_head);

	object->root = NULL;
	object->type = type;
	object->size = size;
	object->generation = 1;
	object->ref_count = 1;
	object->flags = 0;
	if ((object->type == OBJT_DEFAULT) || (object->type == OBJT_SWAP))
		object->flags = OBJ_ONEMAPPING;
	object->pg_color = 0;
	object->handle = NULL;
	object->backing_object = NULL;
	object->backing_object_offset = (vm_ooffset_t) 0;
	object->cache = NULL;

	mtx_lock(&vm_object_list_mtx);
	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	mtx_unlock(&vm_object_list_mtx);
}

/*
 *	vm_object_init:
 *
 *	Initialize the VM objects module.
 */
void
vm_object_init(void)
{
	TAILQ_INIT(&vm_object_list);
	mtx_init(&vm_object_list_mtx, "vm object_list", NULL, MTX_DEF);
	
	VM_OBJECT_LOCK_INIT(&kernel_object_store, "kernel object");
	_vm_object_allocate(OBJT_PHYS, OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS),
	    kernel_object);

	VM_OBJECT_LOCK_INIT(&kmem_object_store, "kmem object");
	_vm_object_allocate(OBJT_PHYS, OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS),
	    kmem_object);

	/*
	 * The lock portion of struct vm_object must be type stable due
	 * to vm_pageout_fallback_object_lock locking a vm object
	 * without holding any references to it.
	 */
	obj_zone = uma_zcreate("VM OBJECT", sizeof (struct vm_object), NULL,
#ifdef INVARIANTS
	    vm_object_zdtor,
#else
	    NULL,
#endif
	    vm_object_zinit, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM|UMA_ZONE_NOFREE);
}

void
vm_object_clear_flag(vm_object_t object, u_short bits)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	object->flags &= ~bits;
}

void
vm_object_pip_add(vm_object_t object, short i)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	object->paging_in_progress += i;
}

void
vm_object_pip_subtract(vm_object_t object, short i)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	object->paging_in_progress -= i;
}

void
vm_object_pip_wakeup(vm_object_t object)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	object->paging_in_progress--;
	if ((object->flags & OBJ_PIPWNT) && object->paging_in_progress == 0) {
		vm_object_clear_flag(object, OBJ_PIPWNT);
		wakeup(object);
	}
}

void
vm_object_pip_wakeupn(vm_object_t object, short i)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (i)
		object->paging_in_progress -= i;
	if ((object->flags & OBJ_PIPWNT) && object->paging_in_progress == 0) {
		vm_object_clear_flag(object, OBJ_PIPWNT);
		wakeup(object);
	}
}

void
vm_object_pip_wait(vm_object_t object, char *waitid)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	while (object->paging_in_progress) {
		object->flags |= OBJ_PIPWNT;
		msleep(object, VM_OBJECT_MTX(object), PVM, waitid, 0);
	}
}

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */
vm_object_t
vm_object_allocate(objtype_t type, vm_pindex_t size)
{
	vm_object_t object;

	object = (vm_object_t)uma_zalloc(obj_zone, M_WAITOK);
	_vm_object_allocate(type, size, object);
	return (object);
}


/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.  Note: OBJ_DEAD
 *	objects can be referenced during final cleaning.
 */
void
vm_object_reference(vm_object_t object)
{
	struct vnode *vp;

	if (object == NULL)
		return;
	VM_OBJECT_LOCK(object);
	object->ref_count++;
	if (object->type == OBJT_VNODE) {
		int vfslocked;

		vp = object->handle;
		VM_OBJECT_UNLOCK(object);
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vget(vp, LK_RETRY, curthread);
		VFS_UNLOCK_GIANT(vfslocked);
	} else
		VM_OBJECT_UNLOCK(object);
}

/*
 *	vm_object_reference_locked:
 *
 *	Gets another reference to the given object.
 *
 *	The object must be locked.
 */
void
vm_object_reference_locked(vm_object_t object)
{
	struct vnode *vp;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT((object->flags & OBJ_DEAD) == 0,
	    ("vm_object_reference_locked: dead object referenced"));
	object->ref_count++;
	if (object->type == OBJT_VNODE) {
		vp = object->handle;
		vref(vp);
	}
}

/*
 * Handle deallocating an object of type OBJT_VNODE.
 */
static void
vm_object_vndeallocate(vm_object_t object)
{
	struct vnode *vp = (struct vnode *) object->handle;

	VFS_ASSERT_GIANT(vp->v_mount);
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT(object->type == OBJT_VNODE,
	    ("vm_object_vndeallocate: not a vnode object"));
	KASSERT(vp != NULL, ("vm_object_vndeallocate: missing vp"));
#ifdef INVARIANTS
	if (object->ref_count == 0) {
		vprint("vm_object_vndeallocate", vp);
		panic("vm_object_vndeallocate: bad object reference count");
	}
#endif

	object->ref_count--;
	if (object->ref_count == 0) {
		mp_fixme("Unlocked vflag access.");
		vp->v_vflag &= ~VV_TEXT;
	}
	VM_OBJECT_UNLOCK(object);
	/*
	 * vrele may need a vop lock
	 */
	vrele(vp);
}

/*
 *	vm_object_deallocate:
 *
 *	Release a reference to the specified object,
 *	gained either through a vm_object_allocate
 *	or a vm_object_reference call.  When all references
 *	are gone, storage associated with this object
 *	may be relinquished.
 *
 *	No object may be locked.
 */
void
vm_object_deallocate(vm_object_t object)
{
	vm_object_t temp;

	while (object != NULL) {
		int vfslocked;

		vfslocked = 0;
	restart:
		VM_OBJECT_LOCK(object);
		if (object->type == OBJT_VNODE) {
			struct vnode *vp = (struct vnode *) object->handle;

			/*
			 * Conditionally acquire Giant for a vnode-backed
			 * object.  We have to be careful since the type of
			 * a vnode object can change while the object is
			 * unlocked.
			 */
			if (VFS_NEEDSGIANT(vp->v_mount) && !vfslocked) {
				vfslocked = 1;
				if (!mtx_trylock(&Giant)) {
					VM_OBJECT_UNLOCK(object);
					mtx_lock(&Giant);
					goto restart;
				}
			}
			vm_object_vndeallocate(object);
			VFS_UNLOCK_GIANT(vfslocked);
			return;
		} else
			/*
			 * This is to handle the case that the object
			 * changed type while we dropped its lock to
			 * obtain Giant.
			 */
			VFS_UNLOCK_GIANT(vfslocked);

		KASSERT(object->ref_count != 0,
			("vm_object_deallocate: object deallocated too many times: %d", object->type));

		/*
		 * If the reference count goes to 0 we start calling
		 * vm_object_terminate() on the object chain.
		 * A ref count of 1 may be a special case depending on the
		 * shadow count being 0 or 1.
		 */
		object->ref_count--;
		if (object->ref_count > 1) {
			VM_OBJECT_UNLOCK(object);
			return;
		} else if (object->ref_count == 1) {
			if (object->shadow_count == 0) {
				vm_object_set_flag(object, OBJ_ONEMAPPING);
			} else if ((object->shadow_count == 1) &&
			    (object->handle == NULL) &&
			    (object->type == OBJT_DEFAULT ||
			     object->type == OBJT_SWAP)) {
				vm_object_t robject;

				robject = LIST_FIRST(&object->shadow_head);
				KASSERT(robject != NULL,
				    ("vm_object_deallocate: ref_count: %d, shadow_count: %d",
					 object->ref_count,
					 object->shadow_count));
				if (!VM_OBJECT_TRYLOCK(robject)) {
					/*
					 * Avoid a potential deadlock.
					 */
					object->ref_count++;
					VM_OBJECT_UNLOCK(object);
					/*
					 * More likely than not the thread
					 * holding robject's lock has lower
					 * priority than the current thread.
					 * Let the lower priority thread run.
					 */
					pause("vmo_de", 1);
					continue;
				}
				/*
				 * Collapse object into its shadow unless its
				 * shadow is dead.  In that case, object will
				 * be deallocated by the thread that is
				 * deallocating its shadow.
				 */
				if ((robject->flags & OBJ_DEAD) == 0 &&
				    (robject->handle == NULL) &&
				    (robject->type == OBJT_DEFAULT ||
				     robject->type == OBJT_SWAP)) {

					robject->ref_count++;
retry:
					if (robject->paging_in_progress) {
						VM_OBJECT_UNLOCK(object);
						vm_object_pip_wait(robject,
						    "objde1");
						temp = robject->backing_object;
						if (object == temp) {
							VM_OBJECT_LOCK(object);
							goto retry;
						}
					} else if (object->paging_in_progress) {
						VM_OBJECT_UNLOCK(robject);
						object->flags |= OBJ_PIPWNT;
						msleep(object,
						    VM_OBJECT_MTX(object),
						    PDROP | PVM, "objde2", 0);
						VM_OBJECT_LOCK(robject);
						temp = robject->backing_object;
						if (object == temp) {
							VM_OBJECT_LOCK(object);
							goto retry;
						}
					} else
						VM_OBJECT_UNLOCK(object);

					if (robject->ref_count == 1) {
						robject->ref_count--;
						object = robject;
						goto doterm;
					}
					object = robject;
					vm_object_collapse(object);
					VM_OBJECT_UNLOCK(object);
					continue;
				}
				VM_OBJECT_UNLOCK(robject);
			}
			VM_OBJECT_UNLOCK(object);
			return;
		}
doterm:
		temp = object->backing_object;
		if (temp != NULL) {
			VM_OBJECT_LOCK(temp);
			LIST_REMOVE(object, shadow_list);
			temp->shadow_count--;
			temp->generation++;
			VM_OBJECT_UNLOCK(temp);
			object->backing_object = NULL;
		}
		/*
		 * Don't double-terminate, we could be in a termination
		 * recursion due to the terminate having to sync data
		 * to disk.
		 */
		if ((object->flags & OBJ_DEAD) == 0)
			vm_object_terminate(object);
		else
			VM_OBJECT_UNLOCK(object);
		object = temp;
	}
}

/*
 *	vm_object_terminate actually destroys the specified object, freeing
 *	up all previously used resources.
 *
 *	The object must be locked.
 *	This routine may block.
 */
void
vm_object_terminate(vm_object_t object)
{
	vm_page_t p;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);

	/*
	 * Make sure no one uses us.
	 */
	vm_object_set_flag(object, OBJ_DEAD);

	/*
	 * wait for the pageout daemon to be done with the object
	 */
	vm_object_pip_wait(object, "objtrm");

	KASSERT(!object->paging_in_progress,
		("vm_object_terminate: pageout in progress"));

	/*
	 * Clean and free the pages, as appropriate. All references to the
	 * object are gone, so we don't need to lock it.
	 */
	if (object->type == OBJT_VNODE) {
		struct vnode *vp = (struct vnode *)object->handle;

		/*
		 * Clean pages and flush buffers.
		 */
		vm_object_page_clean(object, 0, 0, OBJPC_SYNC);
		VM_OBJECT_UNLOCK(object);

		vinvalbuf(vp, V_SAVE, NULL, 0, 0);

		VM_OBJECT_LOCK(object);
	}

	KASSERT(object->ref_count == 0, 
		("vm_object_terminate: object with references, ref_count=%d",
		object->ref_count));

	/*
	 * Now free any remaining pages. For internal objects, this also
	 * removes them from paging queues. Don't free wired pages, just
	 * remove them from the object. 
	 */
	vm_page_lock_queues();
	while ((p = TAILQ_FIRST(&object->memq)) != NULL) {
		KASSERT(!p->busy && (p->oflags & VPO_BUSY) == 0,
			("vm_object_terminate: freeing busy page %p "
			"p->busy = %d, p->flags %x\n", p, p->busy, p->flags));
		if (p->wire_count == 0) {
			vm_page_free(p);
			cnt.v_pfree++;
		} else {
			vm_page_remove(p);
		}
	}
	vm_page_unlock_queues();

	if (__predict_false(object->cache != NULL))
		vm_page_cache_free(object, 0, 0);

	/*
	 * Let the pager know object is dead.
	 */
	vm_pager_deallocate(object);
	VM_OBJECT_UNLOCK(object);

	/*
	 * Remove the object from the global object list.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_REMOVE(&vm_object_list, object, object_list);
	mtx_unlock(&vm_object_list_mtx);

	/*
	 * Free the space for the object.
	 */
	uma_zfree(obj_zone, object);
}

/*
 *	vm_object_page_clean
 *
 *	Clean all dirty pages in the specified range of object.  Leaves page 
 * 	on whatever queue it is currently on.   If NOSYNC is set then do not
 *	write out pages with VPO_NOSYNC set (originally comes from MAP_NOSYNC),
 *	leaving the object dirty.
 *
 *	When stuffing pages asynchronously, allow clustering.  XXX we need a
 *	synchronous clustering mode implementation.
 *
 *	Odd semantics: if start == end, we clean everything.
 *
 *	The object must be locked.
 */
void
vm_object_page_clean(vm_object_t object, vm_pindex_t start, vm_pindex_t end, int flags)
{
	vm_page_t p, np;
	vm_pindex_t tstart, tend;
	vm_pindex_t pi;
	int clearobjflags;
	int pagerflags;
	int curgeneration;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->type != OBJT_VNODE ||
		(object->flags & OBJ_MIGHTBEDIRTY) == 0)
		return;

	pagerflags = (flags & (OBJPC_SYNC | OBJPC_INVAL)) ? VM_PAGER_PUT_SYNC : VM_PAGER_CLUSTER_OK;
	pagerflags |= (flags & OBJPC_INVAL) ? VM_PAGER_PUT_INVAL : 0;

	vm_object_set_flag(object, OBJ_CLEANING);

	tstart = start;
	if (end == 0) {
		tend = object->size;
	} else {
		tend = end;
	}

	vm_page_lock_queues();
	/*
	 * If the caller is smart and only msync()s a range he knows is
	 * dirty, we may be able to avoid an object scan.  This results in
	 * a phenominal improvement in performance.  We cannot do this
	 * as a matter of course because the object may be huge - e.g.
	 * the size might be in the gigabytes or terrabytes.
	 */
	if (msync_flush_flags & MSYNC_FLUSH_HARDSEQ) {
		vm_pindex_t tscan;
		int scanlimit;
		int scanreset;

		scanreset = object->resident_page_count / EASY_SCAN_FACTOR;
		if (scanreset < 16)
			scanreset = 16;
		pagerflags |= VM_PAGER_IGNORE_CLEANCHK;

		scanlimit = scanreset;
		tscan = tstart;
		while (tscan < tend) {
			curgeneration = object->generation;
			p = vm_page_lookup(object, tscan);
			if (p == NULL || p->valid == 0) {
				if (--scanlimit == 0)
					break;
				++tscan;
				continue;
			}
			vm_page_test_dirty(p);
			if ((p->dirty & p->valid) == 0) {
				if (--scanlimit == 0)
					break;
				++tscan;
				continue;
			}
			/*
			 * If we have been asked to skip nosync pages and 
			 * this is a nosync page, we can't continue.
			 */
			if ((flags & OBJPC_NOSYNC) && (p->oflags & VPO_NOSYNC)) {
				if (--scanlimit == 0)
					break;
				++tscan;
				continue;
			}
			scanlimit = scanreset;

			/*
			 * This returns 0 if it was unable to busy the first
			 * page (i.e. had to sleep).
			 */
			tscan += vm_object_page_collect_flush(object, p, curgeneration, pagerflags);
		}

		/*
		 * If everything was dirty and we flushed it successfully,
		 * and the requested range is not the entire object, we
		 * don't have to mess with CLEANCHK or MIGHTBEDIRTY and can
		 * return immediately.
		 */
		if (tscan >= tend && (tstart || tend < object->size)) {
			vm_page_unlock_queues();
			vm_object_clear_flag(object, OBJ_CLEANING);
			return;
		}
		pagerflags &= ~VM_PAGER_IGNORE_CLEANCHK;
	}

	/*
	 * Generally set CLEANCHK interlock and make the page read-only so
	 * we can then clear the object flags.
	 *
	 * However, if this is a nosync mmap then the object is likely to 
	 * stay dirty so do not mess with the page and do not clear the
	 * object flags.
	 */
	clearobjflags = 1;
	TAILQ_FOREACH(p, &object->memq, listq) {
		p->oflags |= VPO_CLEANCHK;
		if ((flags & OBJPC_NOSYNC) && (p->oflags & VPO_NOSYNC))
			clearobjflags = 0;
		else
			pmap_remove_write(p);
	}

	if (clearobjflags && (tstart == 0) && (tend == object->size)) {
		struct vnode *vp;

		vm_object_clear_flag(object, OBJ_MIGHTBEDIRTY);
		if (object->type == OBJT_VNODE &&
		    (vp = (struct vnode *)object->handle) != NULL) {
			VI_LOCK(vp);
			if (vp->v_iflag & VI_OBJDIRTY)
				vp->v_iflag &= ~VI_OBJDIRTY;
			VI_UNLOCK(vp);
		}
	}

rescan:
	curgeneration = object->generation;

	for (p = TAILQ_FIRST(&object->memq); p; p = np) {
		int n;

		np = TAILQ_NEXT(p, listq);

again:
		pi = p->pindex;
		if ((p->oflags & VPO_CLEANCHK) == 0 ||
			(pi < tstart) || (pi >= tend) ||
		    p->valid == 0) {
			p->oflags &= ~VPO_CLEANCHK;
			continue;
		}

		vm_page_test_dirty(p);
		if ((p->dirty & p->valid) == 0) {
			p->oflags &= ~VPO_CLEANCHK;
			continue;
		}

		/*
		 * If we have been asked to skip nosync pages and this is a
		 * nosync page, skip it.  Note that the object flags were
		 * not cleared in this case so we do not have to set them.
		 */
		if ((flags & OBJPC_NOSYNC) && (p->oflags & VPO_NOSYNC)) {
			p->oflags &= ~VPO_CLEANCHK;
			continue;
		}

		n = vm_object_page_collect_flush(object, p,
			curgeneration, pagerflags);
		if (n == 0)
			goto rescan;

		if (object->generation != curgeneration)
			goto rescan;

		/*
		 * Try to optimize the next page.  If we can't we pick up
		 * our (random) scan where we left off.
		 */
		if (msync_flush_flags & MSYNC_FLUSH_SOFTSEQ) {
			if ((p = vm_page_lookup(object, pi + n)) != NULL)
				goto again;
		}
	}
	vm_page_unlock_queues();
#if 0
	VOP_FSYNC(vp, (pagerflags & VM_PAGER_PUT_SYNC)?MNT_WAIT:0, curproc);
#endif

	vm_object_clear_flag(object, OBJ_CLEANING);
	return;
}

static int
vm_object_page_collect_flush(vm_object_t object, vm_page_t p, int curgeneration, int pagerflags)
{
	int runlen;
	int maxf;
	int chkb;
	int maxb;
	int i;
	vm_pindex_t pi;
	vm_page_t maf[vm_pageout_page_count];
	vm_page_t mab[vm_pageout_page_count];
	vm_page_t ma[vm_pageout_page_count];

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	pi = p->pindex;
	while (vm_page_sleep_if_busy(p, TRUE, "vpcwai")) {
		vm_page_lock_queues();
		if (object->generation != curgeneration) {
			return(0);
		}
	}
	maxf = 0;
	for(i = 1; i < vm_pageout_page_count; i++) {
		vm_page_t tp;

		if ((tp = vm_page_lookup(object, pi + i)) != NULL) {
			if ((tp->oflags & VPO_BUSY) ||
				((pagerflags & VM_PAGER_IGNORE_CLEANCHK) == 0 &&
				 (tp->oflags & VPO_CLEANCHK) == 0) ||
				(tp->busy != 0))
				break;
			vm_page_test_dirty(tp);
			if ((tp->dirty & tp->valid) == 0) {
				tp->oflags &= ~VPO_CLEANCHK;
				break;
			}
			maf[ i - 1 ] = tp;
			maxf++;
			continue;
		}
		break;
	}

	maxb = 0;
	chkb = vm_pageout_page_count -  maxf;
	if (chkb) {
		for(i = 1; i < chkb;i++) {
			vm_page_t tp;

			if ((tp = vm_page_lookup(object, pi - i)) != NULL) {
				if ((tp->oflags & VPO_BUSY) ||
					((pagerflags & VM_PAGER_IGNORE_CLEANCHK) == 0 &&
					 (tp->oflags & VPO_CLEANCHK) == 0) ||
					(tp->busy != 0))
					break;
				vm_page_test_dirty(tp);
				if ((tp->dirty & tp->valid) == 0) {
					tp->oflags &= ~VPO_CLEANCHK;
					break;
				}
				mab[ i - 1 ] = tp;
				maxb++;
				continue;
			}
			break;
		}
	}

	for(i = 0; i < maxb; i++) {
		int index = (maxb - i) - 1;
		ma[index] = mab[i];
		ma[index]->oflags &= ~VPO_CLEANCHK;
	}
	p->oflags &= ~VPO_CLEANCHK;
	ma[maxb] = p;
	for(i = 0; i < maxf; i++) {
		int index = (maxb + i) + 1;
		ma[index] = maf[i];
		ma[index]->oflags &= ~VPO_CLEANCHK;
	}
	runlen = maxb + maxf + 1;

	vm_pageout_flush(ma, runlen, pagerflags);
	for (i = 0; i < runlen; i++) {
		if (ma[i]->valid & ma[i]->dirty) {
			pmap_remove_write(ma[i]);
			ma[i]->oflags |= VPO_CLEANCHK;

			/*
			 * maxf will end up being the actual number of pages
			 * we wrote out contiguously, non-inclusive of the
			 * first page.  We do not count look-behind pages.
			 */
			if (i >= maxb + 1 && (maxf > i - maxb - 1))
				maxf = i - maxb - 1;
		}
	}
	return(maxf + 1);
}

/*
 * Note that there is absolutely no sense in writing out
 * anonymous objects, so we track down the vnode object
 * to write out.
 * We invalidate (remove) all pages from the address space
 * for semantic correctness.
 *
 * Note: certain anonymous maps, such as MAP_NOSYNC maps,
 * may start out with a NULL object.
 */
void
vm_object_sync(vm_object_t object, vm_ooffset_t offset, vm_size_t size,
    boolean_t syncio, boolean_t invalidate)
{
	vm_object_t backing_object;
	struct vnode *vp;
	struct mount *mp;
	int flags;

	if (object == NULL)
		return;
	VM_OBJECT_LOCK(object);
	while ((backing_object = object->backing_object) != NULL) {
		VM_OBJECT_LOCK(backing_object);
		offset += object->backing_object_offset;
		VM_OBJECT_UNLOCK(object);
		object = backing_object;
		if (object->size < OFF_TO_IDX(offset + size))
			size = IDX_TO_OFF(object->size) - offset;
	}
	/*
	 * Flush pages if writing is allowed, invalidate them
	 * if invalidation requested.  Pages undergoing I/O
	 * will be ignored by vm_object_page_remove().
	 *
	 * We cannot lock the vnode and then wait for paging
	 * to complete without deadlocking against vm_fault.
	 * Instead we simply call vm_object_page_remove() and
	 * allow it to block internally on a page-by-page
	 * basis when it encounters pages undergoing async
	 * I/O.
	 */
	if (object->type == OBJT_VNODE &&
	    (object->flags & OBJ_MIGHTBEDIRTY) != 0) {
		int vfslocked;
		vp = object->handle;
		VM_OBJECT_UNLOCK(object);
		(void) vn_start_write(vp, &mp, V_WAIT);
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
		flags = (syncio || invalidate) ? OBJPC_SYNC : 0;
		flags |= invalidate ? OBJPC_INVAL : 0;
		VM_OBJECT_LOCK(object);
		vm_object_page_clean(object,
		    OFF_TO_IDX(offset),
		    OFF_TO_IDX(offset + size + PAGE_MASK),
		    flags);
		VM_OBJECT_UNLOCK(object);
		VOP_UNLOCK(vp, 0, curthread);
		VFS_UNLOCK_GIANT(vfslocked);
		vn_finished_write(mp);
		VM_OBJECT_LOCK(object);
	}
	if ((object->type == OBJT_VNODE ||
	     object->type == OBJT_DEVICE) && invalidate) {
		boolean_t purge;
		purge = old_msync || (object->type == OBJT_DEVICE);
		vm_object_page_remove(object,
		    OFF_TO_IDX(offset),
		    OFF_TO_IDX(offset + size + PAGE_MASK),
		    purge ? FALSE : TRUE);
	}
	VM_OBJECT_UNLOCK(object);
}

/*
 *	vm_object_madvise:
 *
 *	Implements the madvise function at the object/page level.
 *
 *	MADV_WILLNEED	(any object)
 *
 *	    Activate the specified pages if they are resident.
 *
 *	MADV_DONTNEED	(any object)
 *
 *	    Deactivate the specified pages if they are resident.
 *
 *	MADV_FREE	(OBJT_DEFAULT/OBJT_SWAP objects,
 *			 OBJ_ONEMAPPING only)
 *
 *	    Deactivate and clean the specified pages if they are
 *	    resident.  This permits the process to reuse the pages
 *	    without faulting or the kernel to reclaim the pages
 *	    without I/O.
 */
void
vm_object_madvise(vm_object_t object, vm_pindex_t pindex, int count, int advise)
{
	vm_pindex_t end, tpindex;
	vm_object_t backing_object, tobject;
	vm_page_t m;

	if (object == NULL)
		return;
	VM_OBJECT_LOCK(object);
	end = pindex + count;
	/*
	 * Locate and adjust resident pages
	 */
	for (; pindex < end; pindex += 1) {
relookup:
		tobject = object;
		tpindex = pindex;
shadowlookup:
		/*
		 * MADV_FREE only operates on OBJT_DEFAULT or OBJT_SWAP pages
		 * and those pages must be OBJ_ONEMAPPING.
		 */
		if (advise == MADV_FREE) {
			if ((tobject->type != OBJT_DEFAULT &&
			     tobject->type != OBJT_SWAP) ||
			    (tobject->flags & OBJ_ONEMAPPING) == 0) {
				goto unlock_tobject;
			}
		}
		m = vm_page_lookup(tobject, tpindex);
		if (m == NULL && advise == MADV_WILLNEED) {
			/*
			 * If the page is cached, reactivate it.
			 */
			m = vm_page_alloc(tobject, tpindex, VM_ALLOC_IFCACHED |
			    VM_ALLOC_NOBUSY);
		}
		if (m == NULL) {
			/*
			 * There may be swap even if there is no backing page
			 */
			if (advise == MADV_FREE && tobject->type == OBJT_SWAP)
				swap_pager_freespace(tobject, tpindex, 1);
			/*
			 * next object
			 */
			backing_object = tobject->backing_object;
			if (backing_object == NULL)
				goto unlock_tobject;
			VM_OBJECT_LOCK(backing_object);
			tpindex += OFF_TO_IDX(tobject->backing_object_offset);
			if (tobject != object)
				VM_OBJECT_UNLOCK(tobject);
			tobject = backing_object;
			goto shadowlookup;
		}
		/*
		 * If the page is busy or not in a normal active state,
		 * we skip it.  If the page is not managed there are no
		 * page queues to mess with.  Things can break if we mess
		 * with pages in any of the below states.
		 */
		vm_page_lock_queues();
		if (m->hold_count ||
		    m->wire_count ||
		    (m->flags & PG_UNMANAGED) ||
		    m->valid != VM_PAGE_BITS_ALL) {
			vm_page_unlock_queues();
			goto unlock_tobject;
		}
		if ((m->oflags & VPO_BUSY) || m->busy) {
			vm_page_flag_set(m, PG_REFERENCED);
			vm_page_unlock_queues();
			if (object != tobject)
				VM_OBJECT_UNLOCK(object);
			m->oflags |= VPO_WANTED;
			msleep(m, VM_OBJECT_MTX(tobject), PDROP | PVM, "madvpo", 0);
			VM_OBJECT_LOCK(object);
  			goto relookup;
		}
		if (advise == MADV_WILLNEED) {
			vm_page_activate(m);
		} else if (advise == MADV_DONTNEED) {
			vm_page_dontneed(m);
		} else if (advise == MADV_FREE) {
			/*
			 * Mark the page clean.  This will allow the page
			 * to be freed up by the system.  However, such pages
			 * are often reused quickly by malloc()/free()
			 * so we do not do anything that would cause
			 * a page fault if we can help it.
			 *
			 * Specifically, we do not try to actually free
			 * the page now nor do we try to put it in the
			 * cache (which would cause a page fault on reuse).
			 *
			 * But we do make the page is freeable as we
			 * can without actually taking the step of unmapping
			 * it.
			 */
			pmap_clear_modify(m);
			m->dirty = 0;
			m->act_count = 0;
			vm_page_dontneed(m);
		}
		vm_page_unlock_queues();
		if (advise == MADV_FREE && tobject->type == OBJT_SWAP)
			swap_pager_freespace(tobject, tpindex, 1);
unlock_tobject:
		if (tobject != object)
			VM_OBJECT_UNLOCK(tobject);
	}	
	VM_OBJECT_UNLOCK(object);
}

/*
 *	vm_object_shadow:
 *
 *	Create a new object which is backed by the
 *	specified existing object range.  The source
 *	object reference is deallocated.
 *
 *	The new object and offset into that object
 *	are returned in the source parameters.
 */
void
vm_object_shadow(
	vm_object_t *object,	/* IN/OUT */
	vm_ooffset_t *offset,	/* IN/OUT */
	vm_size_t length)
{
	vm_object_t source;
	vm_object_t result;

	source = *object;

	/*
	 * Don't create the new object if the old object isn't shared.
	 */
	if (source != NULL) {
		VM_OBJECT_LOCK(source);
		if (source->ref_count == 1 &&
		    source->handle == NULL &&
		    (source->type == OBJT_DEFAULT ||
		     source->type == OBJT_SWAP)) {
			VM_OBJECT_UNLOCK(source);
			return;
		}
		VM_OBJECT_UNLOCK(source);
	}

	/*
	 * Allocate a new object with the given length.
	 */
	result = vm_object_allocate(OBJT_DEFAULT, length);

	/*
	 * The new object shadows the source object, adding a reference to it.
	 * Our caller changes his reference to point to the new object,
	 * removing a reference to the source object.  Net result: no change
	 * of reference count.
	 *
	 * Try to optimize the result object's page color when shadowing
	 * in order to maintain page coloring consistency in the combined 
	 * shadowed object.
	 */
	result->backing_object = source;
	/*
	 * Store the offset into the source object, and fix up the offset into
	 * the new object.
	 */
	result->backing_object_offset = *offset;
	if (source != NULL) {
		VM_OBJECT_LOCK(source);
		LIST_INSERT_HEAD(&source->shadow_head, result, shadow_list);
		source->shadow_count++;
		source->generation++;
		result->flags |= source->flags & OBJ_NEEDGIANT;
		VM_OBJECT_UNLOCK(source);
	}


	/*
	 * Return the new things
	 */
	*offset = 0;
	*object = result;
}

/*
 *	vm_object_split:
 *
 * Split the pages in a map entry into a new object.  This affords
 * easier removal of unused pages, and keeps object inheritance from
 * being a negative impact on memory usage.
 */
void
vm_object_split(vm_map_entry_t entry)
{
	vm_page_t m, m_next;
	vm_object_t orig_object, new_object, source;
	vm_pindex_t idx, offidxstart;
	vm_size_t size;

	orig_object = entry->object.vm_object;
	if (orig_object->type != OBJT_DEFAULT && orig_object->type != OBJT_SWAP)
		return;
	if (orig_object->ref_count <= 1)
		return;
	VM_OBJECT_UNLOCK(orig_object);

	offidxstart = OFF_TO_IDX(entry->offset);
	size = atop(entry->end - entry->start);

	/*
	 * If swap_pager_copy() is later called, it will convert new_object
	 * into a swap object.
	 */
	new_object = vm_object_allocate(OBJT_DEFAULT, size);

	/*
	 * At this point, the new object is still private, so the order in
	 * which the original and new objects are locked does not matter.
	 */
	VM_OBJECT_LOCK(new_object);
	VM_OBJECT_LOCK(orig_object);
	source = orig_object->backing_object;
	if (source != NULL) {
		VM_OBJECT_LOCK(source);
		if ((source->flags & OBJ_DEAD) != 0) {
			VM_OBJECT_UNLOCK(source);
			VM_OBJECT_UNLOCK(orig_object);
			VM_OBJECT_UNLOCK(new_object);
			vm_object_deallocate(new_object);
			VM_OBJECT_LOCK(orig_object);
			return;
		}
		LIST_INSERT_HEAD(&source->shadow_head,
				  new_object, shadow_list);
		source->shadow_count++;
		source->generation++;
		vm_object_reference_locked(source);	/* for new_object */
		vm_object_clear_flag(source, OBJ_ONEMAPPING);
		VM_OBJECT_UNLOCK(source);
		new_object->backing_object_offset = 
			orig_object->backing_object_offset + entry->offset;
		new_object->backing_object = source;
	}
	new_object->flags |= orig_object->flags & OBJ_NEEDGIANT;
retry:
	if ((m = TAILQ_FIRST(&orig_object->memq)) != NULL) {
		if (m->pindex < offidxstart) {
			m = vm_page_splay(offidxstart, orig_object->root);
			if ((orig_object->root = m)->pindex < offidxstart)
				m = TAILQ_NEXT(m, listq);
		}
	}
	vm_page_lock_queues();
	for (; m != NULL && (idx = m->pindex - offidxstart) < size;
	    m = m_next) {
		m_next = TAILQ_NEXT(m, listq);

		/*
		 * We must wait for pending I/O to complete before we can
		 * rename the page.
		 *
		 * We do not have to VM_PROT_NONE the page as mappings should
		 * not be changed by this operation.
		 */
		if ((m->oflags & VPO_BUSY) || m->busy) {
			vm_page_flag_set(m, PG_REFERENCED);
			vm_page_unlock_queues();
			VM_OBJECT_UNLOCK(new_object);
			m->oflags |= VPO_WANTED;
			msleep(m, VM_OBJECT_MTX(orig_object), PVM, "spltwt", 0);
			VM_OBJECT_LOCK(new_object);
			goto retry;
		}
		vm_page_rename(m, new_object, idx);
		/* page automatically made dirty by rename and cache handled */
		vm_page_busy(m);
	}
	vm_page_unlock_queues();
	if (orig_object->type == OBJT_SWAP) {
		/*
		 * swap_pager_copy() can sleep, in which case the orig_object's
		 * and new_object's locks are released and reacquired. 
		 */
		swap_pager_copy(orig_object, new_object, offidxstart, 0);

		/*
		 * Transfer any cached pages from orig_object to new_object.
		 */
		if (__predict_false(orig_object->cache != NULL))
			vm_page_cache_transfer(orig_object, offidxstart,
			    new_object);
	}
	VM_OBJECT_UNLOCK(orig_object);
	TAILQ_FOREACH(m, &new_object->memq, listq)
		vm_page_wakeup(m);
	VM_OBJECT_UNLOCK(new_object);
	entry->object.vm_object = new_object;
	entry->offset = 0LL;
	vm_object_deallocate(orig_object);
	VM_OBJECT_LOCK(new_object);
}

#define	OBSC_TEST_ALL_SHADOWED	0x0001
#define	OBSC_COLLAPSE_NOWAIT	0x0002
#define	OBSC_COLLAPSE_WAIT	0x0004

static int
vm_object_backing_scan(vm_object_t object, int op)
{
	int r = 1;
	vm_page_t p;
	vm_object_t backing_object;
	vm_pindex_t backing_offset_index;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(object->backing_object, MA_OWNED);

	backing_object = object->backing_object;
	backing_offset_index = OFF_TO_IDX(object->backing_object_offset);

	/*
	 * Initial conditions
	 */
	if (op & OBSC_TEST_ALL_SHADOWED) {
		/*
		 * We do not want to have to test for the existence of cache
		 * or swap pages in the backing object.  XXX but with the
		 * new swapper this would be pretty easy to do.
		 *
		 * XXX what about anonymous MAP_SHARED memory that hasn't
		 * been ZFOD faulted yet?  If we do not test for this, the
		 * shadow test may succeed! XXX
		 */
		if (backing_object->type != OBJT_DEFAULT) {
			return (0);
		}
	}
	if (op & OBSC_COLLAPSE_WAIT) {
		vm_object_set_flag(backing_object, OBJ_DEAD);
	}

	/*
	 * Our scan
	 */
	p = TAILQ_FIRST(&backing_object->memq);
	while (p) {
		vm_page_t next = TAILQ_NEXT(p, listq);
		vm_pindex_t new_pindex = p->pindex - backing_offset_index;

		if (op & OBSC_TEST_ALL_SHADOWED) {
			vm_page_t pp;

			/*
			 * Ignore pages outside the parent object's range
			 * and outside the parent object's mapping of the 
			 * backing object.
			 *
			 * note that we do not busy the backing object's
			 * page.
			 */
			if (
			    p->pindex < backing_offset_index ||
			    new_pindex >= object->size
			) {
				p = next;
				continue;
			}

			/*
			 * See if the parent has the page or if the parent's
			 * object pager has the page.  If the parent has the
			 * page but the page is not valid, the parent's
			 * object pager must have the page.
			 *
			 * If this fails, the parent does not completely shadow
			 * the object and we might as well give up now.
			 */

			pp = vm_page_lookup(object, new_pindex);
			if (
			    (pp == NULL || pp->valid == 0) &&
			    !vm_pager_has_page(object, new_pindex, NULL, NULL)
			) {
				r = 0;
				break;
			}
		}

		/*
		 * Check for busy page
		 */
		if (op & (OBSC_COLLAPSE_WAIT | OBSC_COLLAPSE_NOWAIT)) {
			vm_page_t pp;

			if (op & OBSC_COLLAPSE_NOWAIT) {
				if ((p->oflags & VPO_BUSY) ||
				    !p->valid || 
				    p->busy) {
					p = next;
					continue;
				}
			} else if (op & OBSC_COLLAPSE_WAIT) {
				if ((p->oflags & VPO_BUSY) || p->busy) {
					vm_page_lock_queues();
					vm_page_flag_set(p, PG_REFERENCED);
					vm_page_unlock_queues();
					VM_OBJECT_UNLOCK(object);
					p->oflags |= VPO_WANTED;
					msleep(p, VM_OBJECT_MTX(backing_object),
					    PDROP | PVM, "vmocol", 0);
					VM_OBJECT_LOCK(object);
					VM_OBJECT_LOCK(backing_object);
					/*
					 * If we slept, anything could have
					 * happened.  Since the object is
					 * marked dead, the backing offset
					 * should not have changed so we
					 * just restart our scan.
					 */
					p = TAILQ_FIRST(&backing_object->memq);
					continue;
				}
			}

			KASSERT(
			    p->object == backing_object,
			    ("vm_object_backing_scan: object mismatch")
			);

			/*
			 * Destroy any associated swap
			 */
			if (backing_object->type == OBJT_SWAP) {
				swap_pager_freespace(
				    backing_object, 
				    p->pindex,
				    1
				);
			}

			if (
			    p->pindex < backing_offset_index ||
			    new_pindex >= object->size
			) {
				/*
				 * Page is out of the parent object's range, we 
				 * can simply destroy it. 
				 */
				vm_page_lock_queues();
				KASSERT(!pmap_page_is_mapped(p),
				    ("freeing mapped page %p", p));
				if (p->wire_count == 0)
					vm_page_free(p);
				else
					vm_page_remove(p);
				vm_page_unlock_queues();
				p = next;
				continue;
			}

			pp = vm_page_lookup(object, new_pindex);
			if (
			    pp != NULL ||
			    vm_pager_has_page(object, new_pindex, NULL, NULL)
			) {
				/*
				 * page already exists in parent OR swap exists
				 * for this location in the parent.  Destroy 
				 * the original page from the backing object.
				 *
				 * Leave the parent's page alone
				 */
				vm_page_lock_queues();
				KASSERT(!pmap_page_is_mapped(p),
				    ("freeing mapped page %p", p));
				if (p->wire_count == 0)
					vm_page_free(p);
				else
					vm_page_remove(p);
				vm_page_unlock_queues();
				p = next;
				continue;
			}

			/*
			 * Page does not exist in parent, rename the
			 * page from the backing object to the main object. 
			 *
			 * If the page was mapped to a process, it can remain 
			 * mapped through the rename.
			 */
			vm_page_lock_queues();
			vm_page_rename(p, object, new_pindex);
			vm_page_unlock_queues();
			/* page automatically made dirty by rename */
		}
		p = next;
	}
	return (r);
}


/*
 * this version of collapse allows the operation to occur earlier and
 * when paging_in_progress is true for an object...  This is not a complete
 * operation, but should plug 99.9% of the rest of the leaks.
 */
static void
vm_object_qcollapse(vm_object_t object)
{
	vm_object_t backing_object = object->backing_object;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(backing_object, MA_OWNED);

	if (backing_object->ref_count != 1)
		return;

	vm_object_backing_scan(object, OBSC_COLLAPSE_NOWAIT);
}

/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 */
void
vm_object_collapse(vm_object_t object)
{
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	
	while (TRUE) {
		vm_object_t backing_object;

		/*
		 * Verify that the conditions are right for collapse:
		 *
		 * The object exists and the backing object exists.
		 */
		if ((backing_object = object->backing_object) == NULL)
			break;

		/*
		 * we check the backing object first, because it is most likely
		 * not collapsable.
		 */
		VM_OBJECT_LOCK(backing_object);
		if (backing_object->handle != NULL ||
		    (backing_object->type != OBJT_DEFAULT &&
		     backing_object->type != OBJT_SWAP) ||
		    (backing_object->flags & OBJ_DEAD) ||
		    object->handle != NULL ||
		    (object->type != OBJT_DEFAULT &&
		     object->type != OBJT_SWAP) ||
		    (object->flags & OBJ_DEAD)) {
			VM_OBJECT_UNLOCK(backing_object);
			break;
		}

		if (
		    object->paging_in_progress != 0 ||
		    backing_object->paging_in_progress != 0
		) {
			vm_object_qcollapse(object);
			VM_OBJECT_UNLOCK(backing_object);
			break;
		}
		/*
		 * We know that we can either collapse the backing object (if
		 * the parent is the only reference to it) or (perhaps) have
		 * the parent bypass the object if the parent happens to shadow
		 * all the resident pages in the entire backing object.
		 *
		 * This is ignoring pager-backed pages such as swap pages.
		 * vm_object_backing_scan fails the shadowing test in this
		 * case.
		 */
		if (backing_object->ref_count == 1) {
			/*
			 * If there is exactly one reference to the backing
			 * object, we can collapse it into the parent.  
			 */
			vm_object_backing_scan(object, OBSC_COLLAPSE_WAIT);

			/*
			 * Move the pager from backing_object to object.
			 */
			if (backing_object->type == OBJT_SWAP) {
				/*
				 * swap_pager_copy() can sleep, in which case
				 * the backing_object's and object's locks are
				 * released and reacquired.
				 */
				swap_pager_copy(
				    backing_object,
				    object,
				    OFF_TO_IDX(object->backing_object_offset), TRUE);

				/*
				 * Free any cached pages from backing_object.
				 */
				if (__predict_false(backing_object->cache != NULL))
					vm_page_cache_free(backing_object, 0, 0);
			}
			/*
			 * Object now shadows whatever backing_object did.
			 * Note that the reference to 
			 * backing_object->backing_object moves from within 
			 * backing_object to within object.
			 */
			LIST_REMOVE(object, shadow_list);
			backing_object->shadow_count--;
			backing_object->generation++;
			if (backing_object->backing_object) {
				VM_OBJECT_LOCK(backing_object->backing_object);
				LIST_REMOVE(backing_object, shadow_list);
				LIST_INSERT_HEAD(
				    &backing_object->backing_object->shadow_head,
				    object, shadow_list);
				/*
				 * The shadow_count has not changed.
				 */
				backing_object->backing_object->generation++;
				VM_OBJECT_UNLOCK(backing_object->backing_object);
			}
			object->backing_object = backing_object->backing_object;
			object->backing_object_offset +=
			    backing_object->backing_object_offset;

			/*
			 * Discard backing_object.
			 *
			 * Since the backing object has no pages, no pager left,
			 * and no object references within it, all that is
			 * necessary is to dispose of it.
			 */
			KASSERT(backing_object->ref_count == 1, ("backing_object %p was somehow re-referenced during collapse!", backing_object));
			VM_OBJECT_UNLOCK(backing_object);

			mtx_lock(&vm_object_list_mtx);
			TAILQ_REMOVE(
			    &vm_object_list, 
			    backing_object,
			    object_list
			);
			mtx_unlock(&vm_object_list_mtx);

			uma_zfree(obj_zone, backing_object);

			object_collapses++;
		} else {
			vm_object_t new_backing_object;

			/*
			 * If we do not entirely shadow the backing object,
			 * there is nothing we can do so we give up.
			 */
			if (object->resident_page_count != object->size &&
			    vm_object_backing_scan(object,
			    OBSC_TEST_ALL_SHADOWED) == 0) {
				VM_OBJECT_UNLOCK(backing_object);
				break;
			}

			/*
			 * Make the parent shadow the next object in the
			 * chain.  Deallocating backing_object will not remove
			 * it, since its reference count is at least 2.
			 */
			LIST_REMOVE(object, shadow_list);
			backing_object->shadow_count--;
			backing_object->generation++;

			new_backing_object = backing_object->backing_object;
			if ((object->backing_object = new_backing_object) != NULL) {
				VM_OBJECT_LOCK(new_backing_object);
				LIST_INSERT_HEAD(
				    &new_backing_object->shadow_head,
				    object,
				    shadow_list
				);
				new_backing_object->shadow_count++;
				new_backing_object->generation++;
				vm_object_reference_locked(new_backing_object);
				VM_OBJECT_UNLOCK(new_backing_object);
				object->backing_object_offset +=
					backing_object->backing_object_offset;
			}

			/*
			 * Drop the reference count on backing_object. Since
			 * its ref_count was at least 2, it will not vanish.
			 */
			backing_object->ref_count--;
			VM_OBJECT_UNLOCK(backing_object);
			object_bypasses++;
		}

		/*
		 * Try again with this object's new backing object.
		 */
	}
}

/*
 *	vm_object_page_remove:
 *
 *	Removes all physical pages in the given range from the
 *	object's list of pages.  If the range's end is zero, all
 *	physical pages from the range's start to the end of the object
 *	are deleted.
 *
 *	The object must be locked.
 */
void
vm_object_page_remove(vm_object_t object, vm_pindex_t start, vm_pindex_t end,
    boolean_t clean_only)
{
	vm_page_t p, next;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->resident_page_count == 0)
		goto skipmemq;

	/*
	 * Since physically-backed objects do not use managed pages, we can't
	 * remove pages from the object (we must instead remove the page
	 * references, and then destroy the object).
	 */
	KASSERT(object->type != OBJT_PHYS || object == kernel_object ||
	    object == kmem_object,
	    ("attempt to remove pages from a physical object"));

	vm_object_pip_add(object, 1);
again:
	vm_page_lock_queues();
	if ((p = TAILQ_FIRST(&object->memq)) != NULL) {
		if (p->pindex < start) {
			p = vm_page_splay(start, object->root);
			if ((object->root = p)->pindex < start)
				p = TAILQ_NEXT(p, listq);
		}
	}
	/*
	 * Assert: the variable p is either (1) the page with the
	 * least pindex greater than or equal to the parameter pindex
	 * or (2) NULL.
	 */
	for (;
	     p != NULL && (p->pindex < end || end == 0);
	     p = next) {
		next = TAILQ_NEXT(p, listq);

		if (p->wire_count != 0) {
			pmap_remove_all(p);
			if (!clean_only)
				p->valid = 0;
			continue;
		}
		if (vm_page_sleep_if_busy(p, TRUE, "vmopar"))
			goto again;
		if (clean_only && p->valid) {
			pmap_remove_write(p);
			if (p->valid & p->dirty)
				continue;
		}
		pmap_remove_all(p);
		vm_page_free(p);
	}
	vm_page_unlock_queues();
	vm_object_pip_wakeup(object);
skipmemq:
	if (__predict_false(object->cache != NULL))
		vm_page_cache_free(object, start, end);
}

/*
 *	Routine:	vm_object_coalesce
 *	Function:	Coalesces two objects backing up adjoining
 *			regions of memory into a single object.
 *
 *	returns TRUE if objects were combined.
 *
 *	NOTE:	Only works at the moment if the second object is NULL -
 *		if it's not, which object do we lock first?
 *
 *	Parameters:
 *		prev_object	First object to coalesce
 *		prev_offset	Offset into prev_object
 *		prev_size	Size of reference to prev_object
 *		next_size	Size of reference to the second object
 *
 *	Conditions:
 *	The object must *not* be locked.
 */
boolean_t
vm_object_coalesce(vm_object_t prev_object, vm_ooffset_t prev_offset,
	vm_size_t prev_size, vm_size_t next_size)
{
	vm_pindex_t next_pindex;

	if (prev_object == NULL)
		return (TRUE);
	VM_OBJECT_LOCK(prev_object);
	if (prev_object->type != OBJT_DEFAULT &&
	    prev_object->type != OBJT_SWAP) {
		VM_OBJECT_UNLOCK(prev_object);
		return (FALSE);
	}

	/*
	 * Try to collapse the object first
	 */
	vm_object_collapse(prev_object);

	/*
	 * Can't coalesce if: . more than one reference . paged out . shadows
	 * another object . has a copy elsewhere (any of which mean that the
	 * pages not mapped to prev_entry may be in use anyway)
	 */
	if (prev_object->backing_object != NULL) {
		VM_OBJECT_UNLOCK(prev_object);
		return (FALSE);
	}

	prev_size >>= PAGE_SHIFT;
	next_size >>= PAGE_SHIFT;
	next_pindex = OFF_TO_IDX(prev_offset) + prev_size;

	if ((prev_object->ref_count > 1) &&
	    (prev_object->size != next_pindex)) {
		VM_OBJECT_UNLOCK(prev_object);
		return (FALSE);
	}

	/*
	 * Remove any pages that may still be in the object from a previous
	 * deallocation.
	 */
	if (next_pindex < prev_object->size) {
		vm_object_page_remove(prev_object,
				      next_pindex,
				      next_pindex + next_size, FALSE);
		if (prev_object->type == OBJT_SWAP)
			swap_pager_freespace(prev_object,
					     next_pindex, next_size);
	}

	/*
	 * Extend the object if necessary.
	 */
	if (next_pindex + next_size > prev_object->size)
		prev_object->size = next_pindex + next_size;

	VM_OBJECT_UNLOCK(prev_object);
	return (TRUE);
}

void
vm_object_set_writeable_dirty(vm_object_t object)
{
	struct vnode *vp;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if ((object->flags & OBJ_MIGHTBEDIRTY) != 0)
		return;
	vm_object_set_flag(object, OBJ_MIGHTBEDIRTY);
	if (object->type == OBJT_VNODE &&
	    (vp = (struct vnode *)object->handle) != NULL) {
		VI_LOCK(vp);
		vp->v_iflag |= VI_OBJDIRTY;
		VI_UNLOCK(vp);
	}
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <sys/cons.h>

#include <ddb/ddb.h>

static int
_vm_object_in_map(vm_map_t map, vm_object_t object, vm_map_entry_t entry)
{
	vm_map_t tmpm;
	vm_map_entry_t tmpe;
	vm_object_t obj;
	int entcount;

	if (map == 0)
		return 0;

	if (entry == 0) {
		tmpe = map->header.next;
		entcount = map->nentries;
		while (entcount-- && (tmpe != &map->header)) {
			if (_vm_object_in_map(map, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
		tmpm = entry->object.sub_map;
		tmpe = tmpm->header.next;
		entcount = tmpm->nentries;
		while (entcount-- && tmpe != &tmpm->header) {
			if (_vm_object_in_map(tmpm, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if ((obj = entry->object.vm_object) != NULL) {
		for (; obj; obj = obj->backing_object)
			if (obj == object) {
				return 1;
			}
	}
	return 0;
}

static int
vm_object_in_map(vm_object_t object)
{
	struct proc *p;

	/* sx_slock(&allproc_lock); */
	FOREACH_PROC_IN_SYSTEM(p) {
		if (!p->p_vmspace /* || (p->p_flag & (P_SYSTEM|P_WEXIT)) */)
			continue;
		if (_vm_object_in_map(&p->p_vmspace->vm_map, object, 0)) {
			/* sx_sunlock(&allproc_lock); */
			return 1;
		}
	}
	/* sx_sunlock(&allproc_lock); */
	if (_vm_object_in_map(kernel_map, object, 0))
		return 1;
	if (_vm_object_in_map(kmem_map, object, 0))
		return 1;
	if (_vm_object_in_map(pager_map, object, 0))
		return 1;
	if (_vm_object_in_map(buffer_map, object, 0))
		return 1;
	return 0;
}

DB_SHOW_COMMAND(vmochk, vm_object_check)
{
	vm_object_t object;

	/*
	 * make sure that internal objs are in a map somewhere
	 * and none have zero ref counts.
	 */
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		if (object->handle == NULL &&
		    (object->type == OBJT_DEFAULT || object->type == OBJT_SWAP)) {
			if (object->ref_count == 0) {
				db_printf("vmochk: internal obj has zero ref count: %ld\n",
					(long)object->size);
			}
			if (!vm_object_in_map(object)) {
				db_printf(
			"vmochk: internal obj is not in a map: "
			"ref: %d, size: %lu: 0x%lx, backing_object: %p\n",
				    object->ref_count, (u_long)object->size, 
				    (u_long)object->size,
				    (void *)object->backing_object);
			}
		}
	}
}

/*
 *	vm_object_print:	[ debug ]
 */
DB_SHOW_COMMAND(object, vm_object_print_static)
{
	/* XXX convert args. */
	vm_object_t object = (vm_object_t)addr;
	boolean_t full = have_addr;

	vm_page_t p;

	/* XXX count is an (unused) arg.  Avoid shadowing it. */
#define	count	was_count

	int count;

	if (object == NULL)
		return;

	db_iprintf(
	    "Object %p: type=%d, size=0x%jx, res=%d, ref=%d, flags=0x%x\n",
	    object, (int)object->type, (uintmax_t)object->size,
	    object->resident_page_count, object->ref_count, object->flags);
	db_iprintf(" sref=%d, backing_object(%d)=(%p)+0x%jx\n",
	    object->shadow_count, 
	    object->backing_object ? object->backing_object->ref_count : 0,
	    object->backing_object, (uintmax_t)object->backing_object_offset);

	if (!full)
		return;

	db_indent += 2;
	count = 0;
	TAILQ_FOREACH(p, &object->memq, listq) {
		if (count == 0)
			db_iprintf("memory:=");
		else if (count == 6) {
			db_printf("\n");
			db_iprintf(" ...");
			count = 0;
		} else
			db_printf(",");
		count++;

		db_printf("(off=0x%jx,page=0x%jx)",
		    (uintmax_t)p->pindex, (uintmax_t)VM_PAGE_TO_PHYS(p));
	}
	if (count != 0)
		db_printf("\n");
	db_indent -= 2;
}

/* XXX. */
#undef count

/* XXX need this non-static entry for calling from vm_map_print. */
void
vm_object_print(
        /* db_expr_t */ long addr,
	boolean_t have_addr,
	/* db_expr_t */ long count,
	char *modif)
{
	vm_object_print_static(addr, have_addr, count, modif);
}

DB_SHOW_COMMAND(vmopag, vm_object_print_pages)
{
	vm_object_t object;
	int nl = 0;
	int c;

	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		vm_pindex_t idx, fidx;
		vm_pindex_t osize;
		vm_paddr_t pa = -1;
		int rcount;
		vm_page_t m;

		db_printf("new object: %p\n", (void *)object);
		if (nl > 18) {
			c = cngetc();
			if (c != ' ')
				return;
			nl = 0;
		}
		nl++;
		rcount = 0;
		fidx = 0;
		osize = object->size;
		if (osize > 128)
			osize = 128;
		for (idx = 0; idx < osize; idx++) {
			m = vm_page_lookup(object, idx);
			if (m == NULL) {
				if (rcount) {
					db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
						(long)fidx, rcount, (long)pa);
					if (nl > 18) {
						c = cngetc();
						if (c != ' ')
							return;
						nl = 0;
					}
					nl++;
					rcount = 0;
				}
				continue;
			}

				
			if (rcount &&
				(VM_PAGE_TO_PHYS(m) == pa + rcount * PAGE_SIZE)) {
				++rcount;
				continue;
			}
			if (rcount) {
				db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
					(long)fidx, rcount, (long)pa);
				if (nl > 18) {
					c = cngetc();
					if (c != ' ')
						return;
					nl = 0;
				}
				nl++;
			}
			fidx = idx;
			pa = VM_PAGE_TO_PHYS(m);
			rcount = 1;
		}
		if (rcount) {
			db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
				(long)fidx, rcount, (long)pa);
			if (nl > 18) {
				c = cngetc();
				if (c != ' ')
					return;
				nl = 0;
			}
			nl++;
		}
	}
}
#endif /* DDB */
