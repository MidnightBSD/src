/*-
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)device_pager.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

static void dev_pager_init(void);
static vm_object_t dev_pager_alloc(void *, vm_ooffset_t, vm_prot_t,
    vm_ooffset_t, struct ucred *);
static void dev_pager_dealloc(vm_object_t);
static int dev_pager_getpages(vm_object_t, vm_page_t *, int, int);
static void dev_pager_putpages(vm_object_t, vm_page_t *, int, 
		boolean_t, int *);
static boolean_t dev_pager_haspage(vm_object_t, vm_pindex_t, int *,
		int *);
static void dev_pager_free_page(vm_object_t object, vm_page_t m);

/* list of device pager objects */
static struct pagerlst dev_pager_object_list;
/* protect list manipulation */
static struct mtx dev_pager_mtx;

struct pagerops devicepagerops = {
	.pgo_init =	dev_pager_init,
	.pgo_alloc =	dev_pager_alloc,
	.pgo_dealloc =	dev_pager_dealloc,
	.pgo_getpages =	dev_pager_getpages,
	.pgo_putpages =	dev_pager_putpages,
	.pgo_haspage =	dev_pager_haspage,
};

struct pagerops mgtdevicepagerops = {
	.pgo_alloc =	dev_pager_alloc,
	.pgo_dealloc =	dev_pager_dealloc,
	.pgo_getpages =	dev_pager_getpages,
	.pgo_putpages =	dev_pager_putpages,
	.pgo_haspage =	dev_pager_haspage,
};

static int old_dev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color);
static void old_dev_pager_dtor(void *handle);
static int old_dev_pager_fault(vm_object_t object, vm_ooffset_t offset,
    int prot, vm_page_t *mres);

static struct cdev_pager_ops old_dev_pager_ops = {
	.cdev_pg_ctor =	old_dev_pager_ctor,
	.cdev_pg_dtor =	old_dev_pager_dtor,
	.cdev_pg_fault = old_dev_pager_fault
};

static void
dev_pager_init()
{
	TAILQ_INIT(&dev_pager_object_list);
	mtx_init(&dev_pager_mtx, "dev_pager list", NULL, MTX_DEF);
}

vm_object_t
cdev_pager_lookup(void *handle)
{
	vm_object_t object;

	mtx_lock(&dev_pager_mtx);
	object = vm_pager_object_lookup(&dev_pager_object_list, handle);
	mtx_unlock(&dev_pager_mtx);
	return (object);
}

vm_object_t
cdev_pager_allocate(void *handle, enum obj_type tp, struct cdev_pager_ops *ops,
    vm_ooffset_t size, vm_prot_t prot, vm_ooffset_t foff, struct ucred *cred)
{
	vm_object_t object, object1;
	vm_pindex_t pindex;
	u_short color;

	if (tp != OBJT_DEVICE && tp != OBJT_MGTDEVICE)
		return (NULL);

	/*
	 * Offset should be page aligned.
	 */
	if (foff & PAGE_MASK)
		return (NULL);

	size = round_page(size);
	pindex = OFF_TO_IDX(foff + size);

	if (ops->cdev_pg_ctor(handle, size, prot, foff, cred, &color) != 0)
		return (NULL);
	mtx_lock(&dev_pager_mtx);

	/*
	 * Look up pager, creating as necessary.
	 */
	object1 = NULL;
	object = vm_pager_object_lookup(&dev_pager_object_list, handle);
	if (object == NULL) {
		/*
		 * Allocate object and associate it with the pager.  Initialize
		 * the object's pg_color based upon the physical address of the
		 * device's memory.
		 */
		mtx_unlock(&dev_pager_mtx);
		object1 = vm_object_allocate(tp, pindex);
		object1->flags |= OBJ_COLORED;
		object1->pg_color = color;
		object1->handle = handle;
		object1->un_pager.devp.ops = ops;
		TAILQ_INIT(&object1->un_pager.devp.devp_pglist);
		mtx_lock(&dev_pager_mtx);
		object = vm_pager_object_lookup(&dev_pager_object_list, handle);
		if (object != NULL) {
			/*
			 * We raced with other thread while allocating object.
			 */
			if (pindex > object->size)
				object->size = pindex;
		} else {
			object = object1;
			object1 = NULL;
			object->handle = handle;
			TAILQ_INSERT_TAIL(&dev_pager_object_list, object,
			    pager_object_list);
			KASSERT(object->type == tp,
		("Inconsistent device pager type %p %d", object, tp));
		}
	} else {
		if (pindex > object->size)
			object->size = pindex;
	}
	mtx_unlock(&dev_pager_mtx);
	if (object1 != NULL) {
		object1->handle = object1;
		mtx_lock(&dev_pager_mtx);
		TAILQ_INSERT_TAIL(&dev_pager_object_list, object1,
		    pager_object_list);
		mtx_unlock(&dev_pager_mtx);
		vm_object_deallocate(object1);
	}
	return (object);
}

static vm_object_t
dev_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred)
{

	return (cdev_pager_allocate(handle, OBJT_DEVICE, &old_dev_pager_ops,
	    size, prot, foff, cred));
}

void
cdev_pager_free_page(vm_object_t object, vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->type == OBJT_MGTDEVICE) {
		KASSERT((m->oflags & VPO_UNMANAGED) == 0, ("unmanaged %p", m));
		pmap_remove_all(m);
		vm_page_lock(m);
		vm_page_remove(m);
		vm_page_unlock(m);
	} else if (object->type == OBJT_DEVICE)
		dev_pager_free_page(object, m);
}

static void
dev_pager_free_page(vm_object_t object, vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT((object->type == OBJT_DEVICE &&
	    (m->oflags & VPO_UNMANAGED) != 0),
	    ("Managed device or page obj %p m %p", object, m));
	TAILQ_REMOVE(&object->un_pager.devp.devp_pglist, m, pageq);
	vm_page_putfake(m);
}

static void
dev_pager_dealloc(object)
	vm_object_t object;
{
	vm_page_t m;

	VM_OBJECT_UNLOCK(object);
	object->un_pager.devp.ops->cdev_pg_dtor(object->handle);

	mtx_lock(&dev_pager_mtx);
	TAILQ_REMOVE(&dev_pager_object_list, object, pager_object_list);
	mtx_unlock(&dev_pager_mtx);
	VM_OBJECT_LOCK(object);

	if (object->type == OBJT_DEVICE) {
		/*
		 * Free up our fake pages.
		 */
		while ((m = TAILQ_FIRST(&object->un_pager.devp.devp_pglist))
		    != NULL)
			dev_pager_free_page(object, m);
	}
}

static int
dev_pager_getpages(vm_object_t object, vm_page_t *ma, int count, int reqpage)
{
	int error, i;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	error = object->un_pager.devp.ops->cdev_pg_fault(object,
	    IDX_TO_OFF(ma[reqpage]->pindex), PROT_READ, &ma[reqpage]);

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);

	for (i = 0; i < count; i++) {
		if (i != reqpage) {
			vm_page_lock(ma[i]);
			vm_page_free(ma[i]);
			vm_page_unlock(ma[i]);
		}
	}

	if (error == VM_PAGER_OK) {
		KASSERT((object->type == OBJT_DEVICE &&
		     (ma[reqpage]->oflags & VPO_UNMANAGED) != 0) ||
		    (object->type == OBJT_MGTDEVICE &&
		     (ma[reqpage]->oflags & VPO_UNMANAGED) == 0),
		    ("Wrong page type %p %p", ma[reqpage], object));
		if (object->type == OBJT_DEVICE) {
			TAILQ_INSERT_TAIL(&object->un_pager.devp.devp_pglist,
			    ma[reqpage], pageq);
		}
	}

	return (error);
}

static int
old_dev_pager_fault(vm_object_t object, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	vm_pindex_t pidx;
	vm_paddr_t paddr;
	vm_page_t m_paddr, page;
	struct cdev *dev;
	struct cdevsw *csw;
	struct file *fpop;
	struct thread *td;
	vm_memattr_t memattr;
	int ref, ret;

	pidx = OFF_TO_IDX(offset);
	memattr = object->memattr;

	VM_OBJECT_UNLOCK(object);

	dev = object->handle;
	csw = dev_refthread(dev, &ref);
	if (csw == NULL) {
		VM_OBJECT_LOCK(object);
		return (VM_PAGER_FAIL);
	}
	td = curthread;
	fpop = td->td_fpop;
	td->td_fpop = NULL;
	ret = csw->d_mmap(dev, offset, &paddr, prot, &memattr);
	td->td_fpop = fpop;
	dev_relthread(dev, ref);
	if (ret != 0) {
		printf(
	    "WARNING: dev_pager_getpage: map function returns error %d", ret);
		VM_OBJECT_LOCK(object);
		return (VM_PAGER_FAIL);
	}

	/* If "paddr" is a real page, perform a sanity check on "memattr". */
	if ((m_paddr = vm_phys_paddr_to_vm_page(paddr)) != NULL &&
	    pmap_page_get_memattr(m_paddr) != memattr) {
		memattr = pmap_page_get_memattr(m_paddr);
		printf(
	    "WARNING: A device driver has set \"memattr\" inconsistently.\n");
	}
	if (((*mres)->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in result page is a fake page, update it with
		 * the new physical address.
		 */
		page = *mres;
		VM_OBJECT_LOCK(object);
		vm_page_updatefake(page, paddr, memattr);
	} else {
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the all of the original pages.
		 */
		page = vm_page_getfake(paddr, memattr);
		VM_OBJECT_LOCK(object);
		vm_page_lock(*mres);
		vm_page_free(*mres);
		vm_page_unlock(*mres);
		*mres = page;
		vm_page_insert(page, object, pidx);
	}
	page->valid = VM_PAGE_BITS_ALL;
	return (VM_PAGER_OK);
}

static void
dev_pager_putpages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{

	panic("dev_pager_putpage called");
}

static boolean_t
dev_pager_haspage(object, pindex, before, after)
	vm_object_t object;
	vm_pindex_t pindex;
	int *before;
	int *after;
{
	if (before != NULL)
		*before = 0;
	if (after != NULL)
		*after = 0;
	return (TRUE);
}

static int
old_dev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct cdev *dev;
	struct cdevsw *csw;
	vm_memattr_t dummy;
	vm_ooffset_t off;
	vm_paddr_t paddr;
	unsigned int npages;
	int ref;

	/*
	 * Make sure this device can be mapped.
	 */
	dev = handle;
	csw = dev_refthread(dev, &ref);
	if (csw == NULL)
		return (ENXIO);

	/*
	 * Check that the specified range of the device allows the desired
	 * protection.
	 *
	 * XXX assumes VM_PROT_* == PROT_*
	 */
	npages = OFF_TO_IDX(size);
	for (off = foff; npages--; off += PAGE_SIZE) {
		if (csw->d_mmap(dev, off, &paddr, (int)prot, &dummy) != 0) {
			dev_relthread(dev, ref);
			return (EINVAL);
		}
	}

	dev_ref(dev);
	dev_relthread(dev, ref);
	*color = atop(paddr) - OFF_TO_IDX(off - PAGE_SIZE);
	return (0);
}

static void
old_dev_pager_dtor(void *handle)
{

	dev_rel(handle);
}
