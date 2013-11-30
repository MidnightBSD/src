/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/acpica/acpi_pci.c,v 1.26.2.1 2005/11/07 09:53:22 obrien Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>

#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include "pcib_if.h"
#include "pci_if.h"

/* Hooks for the ACPI CA debugging infrastructure. */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI")

struct acpi_pci_devinfo {
	struct pci_devinfo	ap_dinfo;
	ACPI_HANDLE		ap_handle;
	int			ap_flags;
};

ACPI_SERIAL_DECL(pci_powerstate, "ACPI PCI power methods");

/* Be sure that ACPI and PCI power states are equivalent. */
CTASSERT(ACPI_STATE_D0 == PCI_POWERSTATE_D0);
CTASSERT(ACPI_STATE_D1 == PCI_POWERSTATE_D1);
CTASSERT(ACPI_STATE_D2 == PCI_POWERSTATE_D2);
CTASSERT(ACPI_STATE_D3 == PCI_POWERSTATE_D3);

static int	acpi_pci_attach(device_t dev);
static int	acpi_pci_child_location_str_method(device_t cbdev,
		    device_t child, char *buf, size_t buflen);
static int	acpi_pci_probe(device_t dev);
static int	acpi_pci_read_ivar(device_t dev, device_t child, int which,
		    uintptr_t *result);
static int	acpi_pci_write_ivar(device_t dev, device_t child, int which,
		    uintptr_t value);
static ACPI_STATUS acpi_pci_save_handle(ACPI_HANDLE handle, UINT32 level,
		    void *context, void **status);
static int	acpi_pci_set_powerstate_method(device_t dev, device_t child,
		    int state);
static void	acpi_pci_update_device(ACPI_HANDLE handle, device_t pci_child);

static device_method_t acpi_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_pci_probe),
	DEVMETHOD(device_attach,	acpi_pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	pci_suspend),
	DEVMETHOD(device_resume,	pci_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	acpi_pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	acpi_pci_write_ivar),
	DEVMETHOD(bus_driver_added,	pci_driver_added),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD(bus_get_resource_list,pci_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_child_pnpinfo_str, pci_child_pnpinfo_str_method),
	DEVMETHOD(bus_child_location_str, acpi_pci_child_location_str_method),

	/* PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	pci_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, pci_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	pci_enable_io_method),
	DEVMETHOD(pci_disable_io,	pci_disable_io_method),
	DEVMETHOD(pci_get_powerstate,	pci_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	acpi_pci_set_powerstate_method),
	DEVMETHOD(pci_assign_interrupt, pci_assign_interrupt_method),

	{ 0, 0 }
};

static driver_t acpi_pci_driver = {
	"pci",
	acpi_pci_methods,
	0,			/* no softc */
};

DRIVER_MODULE(acpi_pci, pcib, acpi_pci_driver, pci_devclass, 0, 0);
MODULE_DEPEND(acpi_pci, acpi, 1, 1, 1);
MODULE_DEPEND(acpi_pci, pci, 1, 1, 1);
MODULE_VERSION(acpi_pci, 1);

static int
acpi_pci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
    struct acpi_pci_devinfo *dinfo;

    dinfo = device_get_ivars(child);
    switch (which) {
    case ACPI_IVAR_HANDLE:
	*result = (uintptr_t)dinfo->ap_handle;
	return (0);
    case ACPI_IVAR_FLAGS:
	*result = (uintptr_t)dinfo->ap_flags;
	return (0);
    }
    return (pci_read_ivar(dev, child, which, result));
}

static int
acpi_pci_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
    struct acpi_pci_devinfo *dinfo;

    dinfo = device_get_ivars(child);
    switch (which) {
    case ACPI_IVAR_HANDLE:
	dinfo->ap_handle = (ACPI_HANDLE)value;
	return (0);
    case ACPI_IVAR_FLAGS:
	dinfo->ap_flags = (int)value;
	return (0);
    }
    return (pci_write_ivar(dev, child, which, value));
}

static int
acpi_pci_child_location_str_method(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
    struct acpi_pci_devinfo *dinfo = device_get_ivars(child);

    pci_child_location_str_method(cbdev, child, buf, buflen);
    
    if (dinfo->ap_handle) {
	strlcat(buf, " handle=", buflen);
	strlcat(buf, acpi_name(dinfo->ap_handle), buflen);
    }
    return (0);
}

/*
 * PCI power manangement
 */
static int
acpi_pci_set_powerstate_method(device_t dev, device_t child, int state)
{
	ACPI_HANDLE h;
	ACPI_STATUS status;
	int old_state, error;

	error = 0;
	if (state < ACPI_STATE_D0 || state > ACPI_STATE_D3)
		return (EINVAL);

	/*
	 * We set the state using PCI Power Management outside of setting
	 * the ACPI state.  This means that when powering down a device, we
	 * first shut it down using PCI, and then using ACPI, which lets ACPI
	 * try to power down any Power Resources that are now no longer used.
	 * When powering up a device, we let ACPI set the state first so that
	 * it can enable any needed Power Resources before changing the PCI
	 * power state.
	 */
	ACPI_SERIAL_BEGIN(pci_powerstate);
	old_state = pci_get_powerstate(child);
	if (old_state < state) {
		error = pci_set_powerstate_method(dev, child, state);
		if (error)
			goto out;
	}
	h = acpi_get_handle(child);
	status = acpi_pwr_switch_consumer(h, state);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND)
		device_printf(dev,
		    "Failed to set ACPI power state D%d on %s: %s\n",
		    state, acpi_name(h), AcpiFormatException(status));
	if (old_state > state)
		error = pci_set_powerstate_method(dev, child, state);

out:
	ACPI_SERIAL_END(pci_powerstate);
	return (error);
}

static void
acpi_pci_update_device(ACPI_HANDLE handle, device_t pci_child)
{
	ACPI_STATUS status;
	device_t child;

	/*
	 * Lookup and remove the unused device that acpi0 creates when it walks
	 * the namespace creating devices.
	 */
	child = acpi_get_device(handle);
	if (child != NULL) {
		KASSERT(!device_is_alive(child), ("%s: deleting alive child %s",
		    __func__, device_get_nameunit(child)));
		KASSERT(device_get_parent(child) ==
		    devclass_get_device(devclass_find("acpi"), 0),
		    ("%s: child (%s)'s parent is not acpi0", __func__,
		    acpi_name(handle)));
		device_delete_child(device_get_parent(child), child);
	}

	/*
	 * Update ACPI-CA to use the PCI enumerated device_t for this handle.
	 */
	status = AcpiDetachData(handle, acpi_fake_objhandler);
	if (ACPI_FAILURE(status))
		printf("WARNING: Unable to detach object data from %s - %s\n",
		    acpi_name(handle), AcpiFormatException(status));
	status = AcpiAttachData(handle, acpi_fake_objhandler, pci_child);
	if (ACPI_FAILURE(status))
		printf("WARNING: Unable to attach object data to %s - %s\n",
		    acpi_name(handle), AcpiFormatException(status));
}

static ACPI_STATUS
acpi_pci_save_handle(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	struct acpi_pci_devinfo *dinfo;
	device_t *devlist;
	int devcount, i, func, slot;
	UINT32 address;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (ACPI_FAILURE(acpi_GetInteger(handle, "_ADR", &address)))
		return_ACPI_STATUS (AE_OK);
	slot = ACPI_ADR_PCI_SLOT(address);
	func = ACPI_ADR_PCI_FUNC(address);
	if (device_get_children((device_t)context, &devlist, &devcount) != 0)
		return_ACPI_STATUS (AE_OK);
	for (i = 0; i < devcount; i++) {
		dinfo = device_get_ivars(devlist[i]);
		if (dinfo->ap_dinfo.cfg.func == func &&
		    dinfo->ap_dinfo.cfg.slot == slot) {
			dinfo->ap_handle = handle;
			acpi_pci_update_device(handle, devlist[i]);
			break;
		}
	}
	free(devlist, M_TEMP);
	return_ACPI_STATUS (AE_OK);
}

static int
acpi_pci_probe(device_t dev)
{

	if (pcib_get_bus(dev) < 0)
		return (ENXIO);
	if (acpi_get_handle(dev) == NULL)
		return (ENXIO);
	device_set_desc(dev, "ACPI PCI bus");
	return (0);
}

static int
acpi_pci_attach(device_t dev)
{
	int busno;

	/*
	 * Since there can be multiple independantly numbered PCI
	 * busses on some large alpha systems, we can't use the unit
	 * number to decide what bus we are probing. We ask the parent 
	 * pcib what our bus number is.
	 */
	busno = pcib_get_bus(dev);
	if (bootverbose)
		device_printf(dev, "physical bus=%d\n", busno);

	/*
	 * First, PCI devices are added as in the normal PCI bus driver.
	 * Afterwards, the ACPI namespace under the bridge driver is
	 * walked to save ACPI handles to all the devices that appear in
	 * the ACPI namespace as immediate descendants of the bridge.
	 *
	 * XXX: Sometimes PCI devices show up in the ACPI namespace that
	 * pci_add_children() doesn't find.  We currently just ignore
	 * these devices.
	 */
	pci_add_children(dev, busno, sizeof(struct acpi_pci_devinfo));
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, acpi_get_handle(dev), 1,
	    acpi_pci_save_handle, dev, NULL);

	return (bus_generic_attach(dev));
}
