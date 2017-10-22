/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/usb/uhci_pci.c 173947 2007-11-26 18:21:42Z jfv $");

/* Universal Host Controller Interface
 *
 * UHCI spec: http://www.intel.com/
 */

/* The low level controller code for UHCI has been split into
 * PCI probes and UHCI specific code. This was done to facilitate the
 * sharing of code between *BSD's
 */

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

#define PCI_UHCI_VENDORID_INTEL		0x8086
#define PCI_UHCI_VENDORID_VIA		0x1106

#define PCI_UHCI_DEVICEID_PIIX3         0x70208086
static const char *uhci_device_piix3 = "Intel 82371SB (PIIX3) USB controller";

#define PCI_UHCI_DEVICEID_PIIX4         0x71128086
#define PCI_UHCI_DEVICEID_PIIX4E        0x71128086	/* no separate stepping */
static const char *uhci_device_piix4 = "Intel 82371AB/EB (PIIX4) USB controller";

#define PCI_UHCI_DEVICEID_ICH		0x24128086
static const char *uhci_device_ich = "Intel 82801AA (ICH) USB controller";

#define PCI_UHCI_DEVICEID_ICH0		0x24228086
static const char *uhci_device_ich0 = "Intel 82801AB (ICH0) USB controller";

#define PCI_UHCI_DEVICEID_ICH2_A	0x24428086
static const char *uhci_device_ich2_a = "Intel 82801BA/BAM (ICH2) USB controller USB-A";

#define PCI_UHCI_DEVICEID_ICH2_B	0x24448086
static const char *uhci_device_ich2_b = "Intel 82801BA/BAM (ICH2) USB controller USB-B";

#define PCI_UHCI_DEVICEID_ICH3_A	0x24828086
static const char *uhci_device_ich3_a = "Intel 82801CA/CAM (ICH3) USB controller USB-A";

#define PCI_UHCI_DEVICEID_ICH3_B	0x24848086
static const char *uhci_device_ich3_b = "Intel 82801CA/CAM (ICH3) USB controller USB-B";

#define PCI_UHCI_DEVICEID_ICH3_C	0x24878086
static const char *uhci_device_ich3_c = "Intel 82801CA/CAM (ICH3) USB controller USB-C";

#define PCI_UHCI_DEVICEID_ICH4_A	0x24c28086
static const char *uhci_device_ich4_a = "Intel 82801DB (ICH4) USB controller USB-A";

#define PCI_UHCI_DEVICEID_ICH4_B	0x24c48086
static const char *uhci_device_ich4_b = "Intel 82801DB (ICH4) USB controller USB-B";

#define PCI_UHCI_DEVICEID_ICH4_C	0x24c78086
static const char *uhci_device_ich4_c = "Intel 82801DB (ICH4) USB controller USB-C";

#define PCI_UHCI_DEVICEID_ICH5_A	0x24d28086
static const char *uhci_device_ich5_a = "Intel 82801EB (ICH5) USB controller USB-A";

#define PCI_UHCI_DEVICEID_ICH5_B	0x24d48086
static const char *uhci_device_ich5_b = "Intel 82801EB (ICH5) USB controller USB-B";

#define PCI_UHCI_DEVICEID_ICH5_C	0x24d78086
static const char *uhci_device_ich5_c = "Intel 82801EB (ICH5) USB controller USB-C";

#define PCI_UHCI_DEVICEID_ICH5_D	0x24de8086
static const char *uhci_device_ich5_d = "Intel 82801EB (ICH5) USB controller USB-D";

#define PCI_UHCI_DEVICEID_ICH6_A	0x26588086
static const char *uhci_device_ich6_a = "Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-A";

#define PCI_UHCI_DEVICEID_ICH6_B	0x26598086
static const char *uhci_device_ich6_b = "Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-B";

#define PCI_UHCI_DEVICEID_ICH6_C	0x265a8086
static const char *uhci_device_ich6_c = "Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-C";

#define PCI_UHCI_DEVICEID_ICH6_D	0x265b8086
static const char *uhci_device_ich6_d = "Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-D";

#define PCI_UHCI_DEVICEID_63XXESB_1	0x26888086
static const char *uhci_device_esb_1 = "Intel 631XESB/632XESB/3100 USB controller USB-1";

#define PCI_UHCI_DEVICEID_63XXESB_2	0x26898086
static const char *uhci_device_esb_2 = "Intel 631XESB/632XESB/3100 USB controller USB-2";

#define PCI_UHCI_DEVICEID_63XXESB_3	0x268a8086
static const char *uhci_device_esb_3 = "Intel 631XESB/632XESB/3100 USB controller USB-3";

#define PCI_UHCI_DEVICEID_63XXESB_4	0x268b8086
static const char *uhci_device_esb_4 = "Intel 631XESB/632XESB/3100 USB controller USB-4";

#define PCI_UHCI_DEVICEID_440MX		0x719a8086
static const char *uhci_device_440mx = "Intel 82443MX USB controller";

#define PCI_UHCI_DEVICEID_460GX		0x76028086
static const char *uhci_device_460gx = "Intel 82372FB/82468GX USB controller";

#define PCI_UHCI_DEVICEID_VT83C572	0x30381106
static const char *uhci_device_vt83c572 = "VIA 83C572 USB controller";

static const char *uhci_device_generic = "UHCI (generic) USB controller";

#define PCI_UHCI_BASE_REG               0x20


static device_attach_t uhci_pci_attach;
static device_detach_t uhci_pci_detach;
static device_suspend_t uhci_pci_suspend;
static device_resume_t uhci_pci_resume;

static int
uhci_pci_suspend(device_t self)
{
	uhci_softc_t *sc = device_get_softc(self);
	int err;

	err = bus_generic_suspend(self);
	if (err)
		return err;
	uhci_power(PWR_SUSPEND, sc);

	return 0;
}

static int
uhci_pci_resume(device_t self)
{
	uhci_softc_t *sc = device_get_softc(self);

	pci_write_config(self, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN, 2);

	uhci_power(PWR_RESUME, sc);
	bus_generic_resume(self);

	return 0;
}

static const char *
uhci_pci_match(device_t self)
{
	u_int32_t device_id = pci_get_devid(self);

	if (device_id == PCI_UHCI_DEVICEID_PIIX3) {
		return (uhci_device_piix3);
	} else if (device_id == PCI_UHCI_DEVICEID_PIIX4) {
		return (uhci_device_piix4);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH) {
		return (uhci_device_ich);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH0) {
		return (uhci_device_ich0);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH2_A) {
		return (uhci_device_ich2_a);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH2_B) {
		return (uhci_device_ich2_b);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH3_A) {
		return (uhci_device_ich3_a);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH3_B) {
		return (uhci_device_ich3_b);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH3_C) {
		return (uhci_device_ich3_c);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH4_A) {
		return (uhci_device_ich4_a);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH4_B) {
		return (uhci_device_ich4_b);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH4_C) {
		return (uhci_device_ich4_c);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH5_A) {
		return (uhci_device_ich5_a);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH5_B) {
		return (uhci_device_ich5_b);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH5_C) {
		return (uhci_device_ich5_c);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH5_D) {
		return (uhci_device_ich5_d);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH6_A) {
		return (uhci_device_ich6_a);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH6_B) {
		return (uhci_device_ich6_b);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH6_C) {
		return (uhci_device_ich6_c);
	} else if (device_id == PCI_UHCI_DEVICEID_ICH6_D) {
		return (uhci_device_ich6_d);
	} else if (device_id == PCI_UHCI_DEVICEID_63XXESB_1) {
		return (uhci_device_esb_1);
	} else if (device_id == PCI_UHCI_DEVICEID_63XXESB_2) {
		return (uhci_device_esb_2);
	} else if (device_id == PCI_UHCI_DEVICEID_63XXESB_3) {
		return (uhci_device_esb_3);
	} else if (device_id == PCI_UHCI_DEVICEID_63XXESB_4) {
		return (uhci_device_esb_4);
	} else if (device_id == PCI_UHCI_DEVICEID_440MX) {
		return (uhci_device_440mx);
	} else if (device_id == PCI_UHCI_DEVICEID_460GX) {
		return (uhci_device_460gx);
	} else if (device_id == PCI_UHCI_DEVICEID_VT83C572) {
		return (uhci_device_vt83c572);
	} else {
		if (pci_get_class(self) == PCIC_SERIALBUS
		    && pci_get_subclass(self) == PCIS_SERIALBUS_USB
		    && pci_get_progif(self) == PCI_INTERFACE_UHCI) {
			return (uhci_device_generic);
		}
	}

	return NULL;		/* dunno... */
}

static int
uhci_pci_probe(device_t self)
{
	const char *desc = uhci_pci_match(self);

	if (desc) {
		device_set_desc(self, desc);
		return BUS_PROBE_DEFAULT;
	} else {
		return ENXIO;
	}
}

static int
uhci_pci_attach(device_t self)
{
	uhci_softc_t *sc = device_get_softc(self);
	int rid;
	int err;

	pci_enable_busmaster(self);

	rid = PCI_UHCI_BASE_REG;
	sc->io_res = bus_alloc_resource_any(self, SYS_RES_IOPORT, &rid,
	    RF_ACTIVE);
	if (!sc->io_res) {
		device_printf(self, "Could not map ports\n");
		return ENXIO;
	}
	sc->iot = rman_get_bustag(sc->io_res);
	sc->ioh = rman_get_bushandle(sc->io_res);

	/* disable interrupts */
	bus_space_write_2(sc->iot, sc->ioh, UHCI_INTR, 0);

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(self, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		uhci_pci_detach(self);
		return ENXIO;
	}
	sc->sc_bus.bdev = device_add_child(self, "usb", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		uhci_pci_detach(self);
		return ENOMEM;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	/* uhci_pci_match must never return NULL if uhci_pci_probe succeeded */
	device_set_desc(sc->sc_bus.bdev, uhci_pci_match(self));
	switch (pci_get_vendor(self)) {
	case PCI_UHCI_VENDORID_INTEL:
		sprintf(sc->sc_vendor, "Intel");
		break;
	case PCI_UHCI_VENDORID_VIA:
		sprintf(sc->sc_vendor, "VIA");
		break;
	default:
		if (bootverbose)
			device_printf(self, "(New UHCI DeviceId=0x%08x)\n",
			    pci_get_devid(self));
		sprintf(sc->sc_vendor, "(0x%04x)", pci_get_vendor(self));
	}

	switch (pci_read_config(self, PCI_USBREV, 1) & PCI_USBREV_MASK) {
	case PCI_USBREV_PRE_1_0:
		sc->sc_bus.usbrev = USBREV_PRE_1_0;
		break;
	case PCI_USBREV_1_0:
		sc->sc_bus.usbrev = USBREV_1_0;
		break;
	default:
		sc->sc_bus.usbrev = USBREV_UNKNOWN;
		break;
	}

	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_BIO,
	    NULL, (driver_intr_t *) uhci_intr, sc, &sc->ih);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		sc->ih = NULL;
		uhci_pci_detach(self);
		return ENXIO;
	}
	/*
	 * Set the PIRQD enable bit and switch off all the others. We don't
	 * want legacy support to interfere with us XXX Does this also mean
	 * that the BIOS won't touch the keyboard anymore if it is connected
	 * to the ports of the root hub?
	 */
#ifdef USB_DEBUG
	if (pci_read_config(self, PCI_LEGSUP, 2) != PCI_LEGSUP_USBPIRQDEN)
		device_printf(self, "LegSup = 0x%04x\n",
		    pci_read_config(self, PCI_LEGSUP, 2));
#endif
	pci_write_config(self, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN, 2);

	/* Allocate a parent dma tag for DMA maps */
	err = bus_dma_tag_create(bus_get_dma_tag(self), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, USB_DMA_NSEG, BUS_SPACE_MAXSIZE_32BIT, 0,
	    NULL, NULL, &sc->sc_bus.parent_dmatag);
	if (err) {
		device_printf(self, "Could not allocate parent DMA tag (%d)\n",
		    err);
		uhci_pci_detach(self);
		return ENXIO;
	}
	/* Allocate a dma tag for transfer buffers */
	err = bus_dma_tag_create(sc->sc_bus.parent_dmatag, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, USB_DMA_NSEG, BUS_SPACE_MAXSIZE_32BIT, 0,
	    busdma_lock_mutex, &Giant, &sc->sc_bus.buffer_dmatag);
	if (err) {
		device_printf(self, "Could not allocate transfer tag (%d)\n",
		    err);
		uhci_pci_detach(self);
		return ENXIO;
	}

	err = uhci_init(sc);
	if (!err) {
		sc->sc_flags |= UHCI_SCFLG_DONEINIT;
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}

	if (err) {
		device_printf(self, "USB init failed\n");
		uhci_pci_detach(self);
		return EIO;
	}
	return 0;		/* success */
}

int
uhci_pci_detach(device_t self)
{
	uhci_softc_t *sc = device_get_softc(self);

	if (sc->sc_flags & UHCI_SCFLG_DONEINIT) {
		uhci_detach(sc, 0);
		sc->sc_flags &= ~UHCI_SCFLG_DONEINIT;
	}

	if (sc->sc_bus.parent_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_bus.parent_dmatag);
	if (sc->sc_bus.buffer_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_bus.buffer_dmatag);

	if (sc->irq_res && sc->ih) {
		int err = bus_teardown_intr(self, sc->irq_res, sc->ih);

		if (err)
			/* XXX or should we panic? */
			device_printf(self, "Could not tear down irq, %d\n",
			    err);
		sc->ih = NULL;
	}
	if (sc->sc_bus.bdev) {
		device_delete_child(self, sc->sc_bus.bdev);
		sc->sc_bus.bdev = NULL;
	}
	if (sc->irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->irq_res);
		sc->irq_res = NULL;
	}
	if (sc->io_res) {
		bus_release_resource(self, SYS_RES_IOPORT, PCI_UHCI_BASE_REG,
		    sc->io_res);
		sc->io_res = NULL;
		sc->iot = 0;
		sc->ioh = 0;
	}
	return 0;
}


static device_method_t uhci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uhci_pci_probe),
	DEVMETHOD(device_attach, uhci_pci_attach),
	DEVMETHOD(device_detach, uhci_pci_detach),
	DEVMETHOD(device_suspend, uhci_pci_suspend),
	DEVMETHOD(device_resume, uhci_pci_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t uhci_driver = {
	"uhci",
	uhci_methods,
	sizeof(uhci_softc_t),
};

static devclass_t uhci_devclass;

DRIVER_MODULE(uhci, pci, uhci_driver, uhci_devclass, 0, 0);
DRIVER_MODULE(uhci, cardbus, uhci_driver, uhci_devclass, 0, 0);
