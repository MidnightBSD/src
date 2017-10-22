/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/10.0.0/sys/amd64/vmm/vmm_dev.c 256651 2013-10-16 21:52:54Z neel $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/amd64/vmm/vmm_dev.c 256651 2013-10-16 21:52:54Z neel $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/libkern.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/pmap.h>
#include <machine/vmparam.h>

#include <machine/vmm.h>
#include "vmm_lapic.h"
#include "vmm_stat.h"
#include "vmm_mem.h"
#include "io/ppt.h"
#include <machine/vmm_dev.h>

struct vmmdev_softc {
	struct vm	*vm;		/* vm instance cookie */
	struct cdev	*cdev;
	SLIST_ENTRY(vmmdev_softc) link;
	int		flags;
};
#define	VSC_LINKED		0x01

static SLIST_HEAD(, vmmdev_softc) head;

static struct mtx vmmdev_mtx;

static MALLOC_DEFINE(M_VMMDEV, "vmmdev", "vmmdev");

SYSCTL_DECL(_hw_vmm);

static struct vmmdev_softc *
vmmdev_lookup(const char *name)
{
	struct vmmdev_softc *sc;

#ifdef notyet	/* XXX kernel is not compiled with invariants */
	mtx_assert(&vmmdev_mtx, MA_OWNED);
#endif

	SLIST_FOREACH(sc, &head, link) {
		if (strcmp(name, vm_name(sc->vm)) == 0)
			break;
	}

	return (sc);
}

static struct vmmdev_softc *
vmmdev_lookup2(struct cdev *cdev)
{

	return (cdev->si_drv1);
}

static int
vmmdev_rw(struct cdev *cdev, struct uio *uio, int flags)
{
	int error, off, c, prot;
	vm_paddr_t gpa;
	void *hpa, *cookie;
	struct vmmdev_softc *sc;

	static char zerobuf[PAGE_SIZE];

	error = 0;
	sc = vmmdev_lookup2(cdev);
	if (sc == NULL)
		error = ENXIO;

	prot = (uio->uio_rw == UIO_WRITE ? VM_PROT_WRITE : VM_PROT_READ);
	while (uio->uio_resid > 0 && error == 0) {
		gpa = uio->uio_offset;
		off = gpa & PAGE_MASK;
		c = min(uio->uio_resid, PAGE_SIZE - off);

		/*
		 * The VM has a hole in its physical memory map. If we want to
		 * use 'dd' to inspect memory beyond the hole we need to
		 * provide bogus data for memory that lies in the hole.
		 *
		 * Since this device does not support lseek(2), dd(1) will
		 * read(2) blocks of data to simulate the lseek(2).
		 */
		hpa = vm_gpa_hold(sc->vm, gpa, c, prot, &cookie);
		if (hpa == NULL) {
			if (uio->uio_rw == UIO_READ)
				error = uiomove(zerobuf, c, uio);
			else
				error = EFAULT;
		} else {
			error = uiomove(hpa, c, uio);
			vm_gpa_release(cookie);
		}
	}
	return (error);
}

static int
vmmdev_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
	     struct thread *td)
{
	int error, vcpu, state_changed;
	struct vmmdev_softc *sc;
	struct vm_memory_segment *seg;
	struct vm_register *vmreg;
	struct vm_seg_desc* vmsegdesc;
	struct vm_run *vmrun;
	struct vm_event *vmevent;
	struct vm_lapic_irq *vmirq;
	struct vm_capability *vmcap;
	struct vm_pptdev *pptdev;
	struct vm_pptdev_mmio *pptmmio;
	struct vm_pptdev_msi *pptmsi;
	struct vm_pptdev_msix *pptmsix;
	struct vm_nmi *vmnmi;
	struct vm_stats *vmstats;
	struct vm_stat_desc *statdesc;
	struct vm_x2apic *x2apic;
	struct vm_gpa_pte *gpapte;

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL)
		return (ENXIO);

	vcpu = -1;
	state_changed = 0;

	/*
	 * Some VMM ioctls can operate only on vcpus that are not running.
	 */
	switch (cmd) {
	case VM_RUN:
	case VM_GET_REGISTER:
	case VM_SET_REGISTER:
	case VM_GET_SEGMENT_DESCRIPTOR:
	case VM_SET_SEGMENT_DESCRIPTOR:
	case VM_INJECT_EVENT:
	case VM_GET_CAPABILITY:
	case VM_SET_CAPABILITY:
	case VM_PPTDEV_MSI:
	case VM_PPTDEV_MSIX:
	case VM_SET_X2APIC_STATE:
		/*
		 * XXX fragile, handle with care
		 * Assumes that the first field of the ioctl data is the vcpu.
		 */
		vcpu = *(int *)data;
		if (vcpu < 0 || vcpu >= VM_MAXCPU) {
			error = EINVAL;
			goto done;
		}

		error = vcpu_set_state(sc->vm, vcpu, VCPU_FROZEN);
		if (error)
			goto done;

		state_changed = 1;
		break;

	case VM_MAP_PPTDEV_MMIO:
	case VM_BIND_PPTDEV:
	case VM_UNBIND_PPTDEV:
	case VM_MAP_MEMORY:
		/*
		 * ioctls that operate on the entire virtual machine must
		 * prevent all vcpus from running.
		 */
		error = 0;
		for (vcpu = 0; vcpu < VM_MAXCPU; vcpu++) {
			error = vcpu_set_state(sc->vm, vcpu, VCPU_FROZEN);
			if (error)
				break;
		}

		if (error) {
			while (--vcpu >= 0)
				vcpu_set_state(sc->vm, vcpu, VCPU_IDLE);
			goto done;
		}

		state_changed = 2;
		break;

	default:
		break;
	}

	switch(cmd) {
	case VM_RUN:
		vmrun = (struct vm_run *)data;
		error = vm_run(sc->vm, vmrun);
		break;
	case VM_STAT_DESC: {
		statdesc = (struct vm_stat_desc *)data;
		error = vmm_stat_desc_copy(statdesc->index,
					statdesc->desc, sizeof(statdesc->desc));
		break;
	}
	case VM_STATS: {
		CTASSERT(MAX_VM_STATS >= MAX_VMM_STAT_ELEMS);
		vmstats = (struct vm_stats *)data;
		getmicrotime(&vmstats->tv);
		error = vmm_stat_copy(sc->vm, vmstats->cpuid,
				      &vmstats->num_entries, vmstats->statbuf);
		break;
	}
	case VM_PPTDEV_MSI:
		pptmsi = (struct vm_pptdev_msi *)data;
		error = ppt_setup_msi(sc->vm, pptmsi->vcpu,
				      pptmsi->bus, pptmsi->slot, pptmsi->func,
				      pptmsi->destcpu, pptmsi->vector,
				      pptmsi->numvec);
		break;
	case VM_PPTDEV_MSIX:
		pptmsix = (struct vm_pptdev_msix *)data;
		error = ppt_setup_msix(sc->vm, pptmsix->vcpu,
				       pptmsix->bus, pptmsix->slot, 
				       pptmsix->func, pptmsix->idx,
				       pptmsix->msg, pptmsix->vector_control,
				       pptmsix->addr);
		break;
	case VM_MAP_PPTDEV_MMIO:
		pptmmio = (struct vm_pptdev_mmio *)data;
		error = ppt_map_mmio(sc->vm, pptmmio->bus, pptmmio->slot,
				     pptmmio->func, pptmmio->gpa, pptmmio->len,
				     pptmmio->hpa);
		break;
	case VM_BIND_PPTDEV:
		pptdev = (struct vm_pptdev *)data;
		error = vm_assign_pptdev(sc->vm, pptdev->bus, pptdev->slot,
					 pptdev->func);
		break;
	case VM_UNBIND_PPTDEV:
		pptdev = (struct vm_pptdev *)data;
		error = vm_unassign_pptdev(sc->vm, pptdev->bus, pptdev->slot,
					   pptdev->func);
		break;
	case VM_INJECT_EVENT:
		vmevent = (struct vm_event *)data;
		error = vm_inject_event(sc->vm, vmevent->cpuid, vmevent->type,
					vmevent->vector,
					vmevent->error_code,
					vmevent->error_code_valid);
		break;
	case VM_INJECT_NMI:
		vmnmi = (struct vm_nmi *)data;
		error = vm_inject_nmi(sc->vm, vmnmi->cpuid);
		break;
	case VM_LAPIC_IRQ:
		vmirq = (struct vm_lapic_irq *)data;
		error = lapic_set_intr(sc->vm, vmirq->cpuid, vmirq->vector);
		break;
	case VM_MAP_MEMORY:
		seg = (struct vm_memory_segment *)data;
		error = vm_malloc(sc->vm, seg->gpa, seg->len);
		break;
	case VM_GET_MEMORY_SEG:
		seg = (struct vm_memory_segment *)data;
		seg->len = 0;
		(void)vm_gpabase2memseg(sc->vm, seg->gpa, seg);
		error = 0;
		break;
	case VM_GET_REGISTER:
		vmreg = (struct vm_register *)data;
		error = vm_get_register(sc->vm, vmreg->cpuid, vmreg->regnum,
					&vmreg->regval);
		break;
	case VM_SET_REGISTER:
		vmreg = (struct vm_register *)data;
		error = vm_set_register(sc->vm, vmreg->cpuid, vmreg->regnum,
					vmreg->regval);
		break;
	case VM_SET_SEGMENT_DESCRIPTOR:
		vmsegdesc = (struct vm_seg_desc *)data;
		error = vm_set_seg_desc(sc->vm, vmsegdesc->cpuid,
					vmsegdesc->regnum,
					&vmsegdesc->desc);
		break;
	case VM_GET_SEGMENT_DESCRIPTOR:
		vmsegdesc = (struct vm_seg_desc *)data;
		error = vm_get_seg_desc(sc->vm, vmsegdesc->cpuid,
					vmsegdesc->regnum,
					&vmsegdesc->desc);
		break;
	case VM_GET_CAPABILITY:
		vmcap = (struct vm_capability *)data;
		error = vm_get_capability(sc->vm, vmcap->cpuid,
					  vmcap->captype,
					  &vmcap->capval);
		break;
	case VM_SET_CAPABILITY:
		vmcap = (struct vm_capability *)data;
		error = vm_set_capability(sc->vm, vmcap->cpuid,
					  vmcap->captype,
					  vmcap->capval);
		break;
	case VM_SET_X2APIC_STATE:
		x2apic = (struct vm_x2apic *)data;
		error = vm_set_x2apic_state(sc->vm,
					    x2apic->cpuid, x2apic->state);
		break;
	case VM_GET_X2APIC_STATE:
		x2apic = (struct vm_x2apic *)data;
		error = vm_get_x2apic_state(sc->vm,
					    x2apic->cpuid, &x2apic->state);
		break;
	case VM_GET_GPA_PMAP:
		gpapte = (struct vm_gpa_pte *)data;
		pmap_get_mapping(vmspace_pmap(vm_get_vmspace(sc->vm)),
				 gpapte->gpa, gpapte->pte, &gpapte->ptenum);
		error = 0;
		break;
	default:
		error = ENOTTY;
		break;
	}

	if (state_changed == 1) {
		vcpu_set_state(sc->vm, vcpu, VCPU_IDLE);
	} else if (state_changed == 2) {
		for (vcpu = 0; vcpu < VM_MAXCPU; vcpu++)
			vcpu_set_state(sc->vm, vcpu, VCPU_IDLE);
	}

done:
	/* Make sure that no handler returns a bogus value like ERESTART */
	KASSERT(error >= 0, ("vmmdev_ioctl: invalid error return %d", error));
	return (error);
}

static int
vmmdev_mmap_single(struct cdev *cdev, vm_ooffset_t *offset,
		   vm_size_t size, struct vm_object **object, int nprot)
{
	int error;
	struct vmmdev_softc *sc;

	sc = vmmdev_lookup2(cdev);
	if (sc != NULL && (nprot & PROT_EXEC) == 0)
		error = vm_get_memobj(sc->vm, *offset, size, offset, object);
	else
		error = EINVAL;

	return (error);
}

static void
vmmdev_destroy(void *arg)
{

	struct vmmdev_softc *sc = arg;

	if (sc->cdev != NULL)
		destroy_dev(sc->cdev);

	if (sc->vm != NULL)
		vm_destroy(sc->vm);

	if ((sc->flags & VSC_LINKED) != 0) {
		mtx_lock(&vmmdev_mtx);
		SLIST_REMOVE(&head, sc, vmmdev_softc, link);
		mtx_unlock(&vmmdev_mtx);
	}

	free(sc, M_VMMDEV);
}

static int
sysctl_vmm_destroy(SYSCTL_HANDLER_ARGS)
{
	int error;
	char buf[VM_MAX_NAMELEN];
	struct vmmdev_softc *sc;
	struct cdev *cdev;

	strlcpy(buf, "beavis", sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	mtx_lock(&vmmdev_mtx);
	sc = vmmdev_lookup(buf);
	if (sc == NULL || sc->cdev == NULL) {
		mtx_unlock(&vmmdev_mtx);
		return (EINVAL);
	}

	/*
	 * The 'cdev' will be destroyed asynchronously when 'si_threadcount'
	 * goes down to 0 so we should not do it again in the callback.
	 */
	cdev = sc->cdev;
	sc->cdev = NULL;		
	mtx_unlock(&vmmdev_mtx);

	/*
	 * Schedule the 'cdev' to be destroyed:
	 *
	 * - any new operations on this 'cdev' will return an error (ENXIO).
	 *
	 * - when the 'si_threadcount' dwindles down to zero the 'cdev' will
	 *   be destroyed and the callback will be invoked in a taskqueue
	 *   context.
	 */
	destroy_dev_sched_cb(cdev, vmmdev_destroy, sc);

	return (0);
}
SYSCTL_PROC(_hw_vmm, OID_AUTO, destroy, CTLTYPE_STRING | CTLFLAG_RW,
	    NULL, 0, sysctl_vmm_destroy, "A", NULL);

static struct cdevsw vmmdevsw = {
	.d_name		= "vmmdev",
	.d_version	= D_VERSION,
	.d_ioctl	= vmmdev_ioctl,
	.d_mmap_single	= vmmdev_mmap_single,
	.d_read		= vmmdev_rw,
	.d_write	= vmmdev_rw,
};

static int
sysctl_vmm_create(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct vm *vm;
	struct cdev *cdev;
	struct vmmdev_softc *sc, *sc2;
	char buf[VM_MAX_NAMELEN];

	strlcpy(buf, "beavis", sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	mtx_lock(&vmmdev_mtx);
	sc = vmmdev_lookup(buf);
	mtx_unlock(&vmmdev_mtx);
	if (sc != NULL)
		return (EEXIST);

	error = vm_create(buf, &vm);
	if (error != 0)
		return (error);

	sc = malloc(sizeof(struct vmmdev_softc), M_VMMDEV, M_WAITOK | M_ZERO);
	sc->vm = vm;

	/*
	 * Lookup the name again just in case somebody sneaked in when we
	 * dropped the lock.
	 */
	mtx_lock(&vmmdev_mtx);
	sc2 = vmmdev_lookup(buf);
	if (sc2 == NULL) {
		SLIST_INSERT_HEAD(&head, sc, link);
		sc->flags |= VSC_LINKED;
	}
	mtx_unlock(&vmmdev_mtx);

	if (sc2 != NULL) {
		vmmdev_destroy(sc);
		return (EEXIST);
	}

	error = make_dev_p(MAKEDEV_CHECKNAME, &cdev, &vmmdevsw, NULL,
			   UID_ROOT, GID_WHEEL, 0600, "vmm/%s", buf);
	if (error != 0) {
		vmmdev_destroy(sc);
		return (error);
	}

	mtx_lock(&vmmdev_mtx);
	sc->cdev = cdev;
	sc->cdev->si_drv1 = sc;
	mtx_unlock(&vmmdev_mtx);

	return (0);
}
SYSCTL_PROC(_hw_vmm, OID_AUTO, create, CTLTYPE_STRING | CTLFLAG_RW,
	    NULL, 0, sysctl_vmm_create, "A", NULL);

void
vmmdev_init(void)
{
	mtx_init(&vmmdev_mtx, "vmm device mutex", NULL, MTX_DEF);
}

int
vmmdev_cleanup(void)
{
	int error;

	if (SLIST_EMPTY(&head))
		error = 0;
	else
		error = EBUSY;

	return (error);
}
