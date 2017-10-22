/*-
 * Copyright (c) 2010 Justin T. Gibbs, Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

/*-
 * PV suspend/resume support:
 *
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2004-2006,2008 Kip Macy
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian Limpach.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * HVM suspend/resume support:
 *
 * Copyright (c) 2008 Citrix Systems, Inc.
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
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/xen/control/control.c 258995 2013-12-05 18:06:12Z royger $");

/**
 * \file control.c
 *
 * \brief Device driver to repond to control domain events that impact
 *        this VM.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/kdb.h>
#include <sys/module.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/rman.h>
#include <sys/sched.h>
#include <sys/taskqueue.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/eventhandler.h>

#include <geom/geom.h>

#include <machine/_inttypes.h>
#include <machine/intr_machdep.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <xen/xen-os.h>
#include <xen/blkif.h>
#include <xen/evtchn.h>
#include <xen/gnttab.h>
#include <xen/xen_intr.h>

#ifdef XENHVM
#include <xen/hvm.h>
#endif

#include <xen/interface/event_channel.h>
#include <xen/interface/grant_table.h>

#include <xen/xenbus/xenbusvar.h>

#include <machine/xen/xenvar.h>
#include <machine/xen/xenfunc.h>

/*--------------------------- Forward Declarations --------------------------*/
/** Function signature for shutdown event handlers. */
typedef	void (xctrl_shutdown_handler_t)(void);

static xctrl_shutdown_handler_t xctrl_poweroff;
static xctrl_shutdown_handler_t xctrl_reboot;
static xctrl_shutdown_handler_t xctrl_suspend;
static xctrl_shutdown_handler_t xctrl_crash;

/*-------------------------- Private Data Structures -------------------------*/
/** Element type for lookup table of event name to handler. */
struct xctrl_shutdown_reason {
	const char		 *name;
	xctrl_shutdown_handler_t *handler;
};

/** Lookup table for shutdown event name to handler. */
static const struct xctrl_shutdown_reason xctrl_shutdown_reasons[] = {
	{ "poweroff", xctrl_poweroff },
	{ "reboot",   xctrl_reboot   },
	{ "suspend",  xctrl_suspend  },
	{ "crash",    xctrl_crash    },
	{ "halt",     xctrl_poweroff },
};

struct xctrl_softc {
	struct xs_watch    xctrl_watch;	
};

/*------------------------------ Event Handlers ------------------------------*/
static void
xctrl_poweroff()
{
	shutdown_nice(RB_POWEROFF|RB_HALT);
}

static void
xctrl_reboot()
{
	shutdown_nice(0);
}

#ifndef XENHVM
extern void xencons_suspend(void);
extern void xencons_resume(void);

/* Full PV mode suspension. */
static void
xctrl_suspend()
{
	int i, j, k, fpp, suspend_cancelled;
	unsigned long max_pfn, start_info_mfn;

	EVENTHANDLER_INVOKE(power_suspend);

#ifdef SMP
	struct thread *td;
	cpuset_t map;
	u_int cpuid;

	/*
	 * Bind us to CPU 0 and stop any other VCPUs.
	 */
	td = curthread;
	thread_lock(td);
	sched_bind(td, 0);
	thread_unlock(td);
	cpuid = PCPU_GET(cpuid);
	KASSERT(cpuid == 0, ("xen_suspend: not running on cpu 0"));

	map = all_cpus;
	CPU_CLR(cpuid, &map);
	CPU_NAND(&map, &stopped_cpus);
	if (!CPU_EMPTY(&map))
		stop_cpus(map);
#endif

	/*
	 * Be sure to hold Giant across DEVICE_SUSPEND/RESUME since non-MPSAFE
	 * drivers need this.
	 */
	mtx_lock(&Giant);
	if (DEVICE_SUSPEND(root_bus) != 0) {
		mtx_unlock(&Giant);
		printf("%s: device_suspend failed\n", __func__);
#ifdef SMP
		if (!CPU_EMPTY(&map))
			restart_cpus(map);
#endif
		return;
	}
	mtx_unlock(&Giant);

	local_irq_disable();

	xencons_suspend();
	gnttab_suspend();
	intr_suspend();

	max_pfn = HYPERVISOR_shared_info->arch.max_pfn;

	void *shared_info = HYPERVISOR_shared_info;
	HYPERVISOR_shared_info = NULL;
	pmap_kremove((vm_offset_t) shared_info);
	PT_UPDATES_FLUSH();

	xen_start_info->store_mfn = MFNTOPFN(xen_start_info->store_mfn);
	xen_start_info->console.domU.mfn = MFNTOPFN(xen_start_info->console.domU.mfn);

	/*
	 * We'll stop somewhere inside this hypercall. When it returns,
	 * we'll start resuming after the restore.
	 */
	start_info_mfn = VTOMFN(xen_start_info);
	pmap_suspend();
	suspend_cancelled = HYPERVISOR_suspend(start_info_mfn);
	pmap_resume();

	pmap_kenter_ma((vm_offset_t) shared_info, xen_start_info->shared_info);
	HYPERVISOR_shared_info = shared_info;

	HYPERVISOR_shared_info->arch.pfn_to_mfn_frame_list_list =
		VTOMFN(xen_pfn_to_mfn_frame_list_list);
  
	fpp = PAGE_SIZE/sizeof(unsigned long);
	for (i = 0, j = 0, k = -1; i < max_pfn; i += fpp, j++) {
		if ((j % fpp) == 0) {
			k++;
			xen_pfn_to_mfn_frame_list_list[k] = 
				VTOMFN(xen_pfn_to_mfn_frame_list[k]);
			j = 0;
		}
		xen_pfn_to_mfn_frame_list[k][j] = 
			VTOMFN(&xen_phys_machine[i]);
	}
	HYPERVISOR_shared_info->arch.max_pfn = max_pfn;

	gnttab_resume();
	intr_resume(suspend_cancelled != 0);
	local_irq_enable();
	xencons_resume();

#ifdef CONFIG_SMP
	for_each_cpu(i)
		vcpu_prepare(i);

#endif

	/* 
	 * Only resume xenbus /after/ we've prepared our VCPUs; otherwise
	 * the VCPU hotplug callback can race with our vcpu_prepare
	 */
	mtx_lock(&Giant);
	DEVICE_RESUME(root_bus);
	mtx_unlock(&Giant);

#ifdef SMP
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);
	if (!CPU_EMPTY(&map))
		restart_cpus(map);
#endif
	EVENTHANDLER_INVOKE(power_resume);
}

static void
xen_pv_shutdown_final(void *arg, int howto)
{
	/*
	 * Inform the hypervisor that shutdown is complete.
	 * This is not necessary in HVM domains since Xen
	 * emulates ACPI in that mode and FreeBSD's ACPI
	 * support will request this transition.
	 */
	if (howto & (RB_HALT | RB_POWEROFF))
		HYPERVISOR_shutdown(SHUTDOWN_poweroff);
	else
		HYPERVISOR_shutdown(SHUTDOWN_reboot);
}

#else

/* HVM mode suspension. */
static void
xctrl_suspend()
{
#ifdef SMP
	cpuset_t cpu_suspend_map;
#endif
	int suspend_cancelled;

	EVENTHANDLER_INVOKE(power_suspend);

	if (smp_started) {
		thread_lock(curthread);
		sched_bind(curthread, 0);
		thread_unlock(curthread);
	}
	KASSERT((PCPU_GET(cpuid) == 0), ("Not running on CPU#0"));

	/*
	 * Clear our XenStore node so the toolstack knows we are
	 * responding to the suspend request.
	 */
	xs_write(XST_NIL, "control", "shutdown", "");

	/*
	 * Be sure to hold Giant across DEVICE_SUSPEND/RESUME since non-MPSAFE
	 * drivers need this.
	 */
	mtx_lock(&Giant);
	if (DEVICE_SUSPEND(root_bus) != 0) {
		mtx_unlock(&Giant);
		printf("%s: device_suspend failed\n", __func__);
		return;
	}
	mtx_unlock(&Giant);

#ifdef SMP
	CPU_ZERO(&cpu_suspend_map);	/* silence gcc */
	if (smp_started) {
		/*
		 * Suspend other CPUs. This prevents IPIs while we
		 * are resuming, and will allow us to reset per-cpu
		 * vcpu_info on resume.
		 */
		cpu_suspend_map = all_cpus;
		CPU_CLR(PCPU_GET(cpuid), &cpu_suspend_map);
		if (!CPU_EMPTY(&cpu_suspend_map))
			suspend_cpus(cpu_suspend_map);
	}
#endif

	/*
	 * Prevent any races with evtchn_interrupt() handler.
	 */
	disable_intr();
	intr_suspend();
	xen_hvm_suspend();

	suspend_cancelled = HYPERVISOR_suspend(0);

	xen_hvm_resume(suspend_cancelled != 0);
	intr_resume(suspend_cancelled != 0);
	enable_intr();

	/*
	 * Reset grant table info.
	 */
	gnttab_resume();

#ifdef SMP
	if (smp_started && !CPU_EMPTY(&cpu_suspend_map)) {
		/*
		 * Now that event channels have been initialized,
		 * resume CPUs.
		 */
		resume_cpus(cpu_suspend_map);
	}
#endif

	/*
	 * FreeBSD really needs to add DEVICE_SUSPEND_CANCEL or
	 * similar.
	 */
	mtx_lock(&Giant);
	DEVICE_RESUME(root_bus);
	mtx_unlock(&Giant);

	if (smp_started) {
		thread_lock(curthread);
		sched_unbind(curthread);
		thread_unlock(curthread);
	}

	EVENTHANDLER_INVOKE(power_resume);

	if (bootverbose)
		printf("System resumed after suspension\n");

}
#endif

static void
xctrl_crash()
{
	panic("Xen directed crash");
}

/*------------------------------ Event Reception -----------------------------*/
static void
xctrl_on_watch_event(struct xs_watch *watch, const char **vec, unsigned int len)
{
	const struct xctrl_shutdown_reason *reason;
	const struct xctrl_shutdown_reason *last_reason;
	char *result;
	int   error;
	int   result_len;
	
	error = xs_read(XST_NIL, "control", "shutdown",
			&result_len, (void **)&result);
	if (error != 0)
		return;

	reason = xctrl_shutdown_reasons;
	last_reason = reason + nitems(xctrl_shutdown_reasons);
	while (reason < last_reason) {

		if (!strcmp(result, reason->name)) {
			reason->handler();
			break;
		}
		reason++;
	}

	free(result, M_XENSTORE);
}

/*------------------ Private Device Attachment Functions  --------------------*/
/**
 * \brief Identify instances of this device type in the system.
 *
 * \param driver  The driver performing this identify action.
 * \param parent  The NewBus parent device for any devices this method adds.
 */
static void
xctrl_identify(driver_t *driver __unused, device_t parent)
{
	/*
	 * A single device instance for our driver is always present
	 * in a system operating under Xen.
	 */
	BUS_ADD_CHILD(parent, 0, driver->name, 0);
}

/**
 * \brief Probe for the existance of the Xen Control device
 *
 * \param dev  NewBus device_t for this Xen control instance.
 *
 * \return  Always returns 0 indicating success.
 */
static int 
xctrl_probe(device_t dev)
{
	device_set_desc(dev, "Xen Control Device");

	return (0);
}

/**
 * \brief Attach the Xen control device.
 *
 * \param dev  NewBus device_t for this Xen control instance.
 *
 * \return  On success, 0. Otherwise an errno value indicating the
 *          type of failure.
 */
static int
xctrl_attach(device_t dev)
{
	struct xctrl_softc *xctrl;

	xctrl = device_get_softc(dev);

	/* Activate watch */
	xctrl->xctrl_watch.node = "control/shutdown";
	xctrl->xctrl_watch.callback = xctrl_on_watch_event;
	xctrl->xctrl_watch.callback_data = (uintptr_t)xctrl;
	xs_register_watch(&xctrl->xctrl_watch);

#ifndef XENHVM
	EVENTHANDLER_REGISTER(shutdown_final, xen_pv_shutdown_final, NULL,
			      SHUTDOWN_PRI_LAST);
#endif

	return (0);
}

/**
 * \brief Detach the Xen control device.
 *
 * \param dev  NewBus device_t for this Xen control device instance.
 *
 * \return  On success, 0. Otherwise an errno value indicating the
 *          type of failure.
 */
static int
xctrl_detach(device_t dev)
{
	struct xctrl_softc *xctrl;

	xctrl = device_get_softc(dev);

	/* Release watch */
	xs_unregister_watch(&xctrl->xctrl_watch);

	return (0);
}

/*-------------------- Private Device Attachment Data  -----------------------*/
static device_method_t xctrl_methods[] = { 
	/* Device interface */ 
	DEVMETHOD(device_identify,	xctrl_identify),
	DEVMETHOD(device_probe,         xctrl_probe), 
	DEVMETHOD(device_attach,        xctrl_attach), 
	DEVMETHOD(device_detach,        xctrl_detach), 
 
	DEVMETHOD_END
}; 

DEFINE_CLASS_0(xctrl, xctrl_driver, xctrl_methods, sizeof(struct xctrl_softc));
devclass_t xctrl_devclass; 
 
DRIVER_MODULE(xctrl, xenstore, xctrl_driver, xctrl_devclass, NULL, NULL);
