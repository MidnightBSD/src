#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/sys/mips/rt305x/rt305x_dotg.c 331722 2018-03-29 02:50:57Z eadler $");

/*-
 * Copyright (c) 2015 Stanislav Galabov. All rights reserved.
 * Copyright (c) 2010,2011 Aleksandr Rybalko. All rights reserved.
 * Copyright (c) 2007-2008 Hans Petter Selasky. All rights reserved.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/rman.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include <dev/usb/controller/dwc_otg.h>
#include <mips/rt305x/rt305xreg.h>
#include <mips/rt305x/rt305x_sysctlvar.h>

#define	MEM_RID	0

static device_probe_t dotg_obio_probe;
static device_attach_t dotg_obio_attach;
static device_detach_t dotg_obio_detach;

static int
dotg_obio_probe(device_t dev)
{
	device_set_desc(dev, "DWC like USB OTG controller");
	return (0);
}

static int
dotg_obio_attach(device_t dev)
{
	struct dwc_otg_softc *sc = device_get_softc(dev);
	uint32_t tmp;
	int err, rid;

	/* setup controller interface softc */

	/* initialise some bus fields */
	sc->sc_mode = DWC_MODE_HOST;
	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = DWC_OTG_MAX_DEVICES;
	sc->sc_bus.dma_bits = 32;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus,
	    USB_GET_DMA_TAG(dev), NULL)) {
		printf("No mem\n");
		return (ENOMEM);
	}
	rid = 0;
	sc->sc_io_res =
	    bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!(sc->sc_io_res)) {
		printf("Can`t alloc MEM\n");
		goto error;
	}
	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, 
	    &rid, RF_ACTIVE);
	if (!(sc->sc_irq_res)) {
		printf("Can`t alloc IRQ\n");
		goto error;
	}

	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_bus.bdev)) {
		printf("Can`t add usbus\n");
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_irq_res,
	    INTR_TYPE_TTY | INTR_MPSAFE, dwc_otg_filter_interrupt,
	    dwc_otg_interrupt, sc, &sc->sc_intr_hdl);
#else
	#error error
	err = bus_setup_intr(dev, sc->sc_irq_res,
	    INTR_TYPE_BIO | INTR_MPSAFE,(driver_intr_t*)dwc_otg_interrupt,
	    sc, &sc->sc_intr_hdl);
#endif
	if (err) {
		sc->sc_intr_hdl = NULL;
		printf("Can`t set IRQ handle\n");
		goto error;
	}

	/* Run clock for OTG core */
	rt305x_sysctl_set(SYSCTL_CLKCFG1, rt305x_sysctl_get(SYSCTL_CLKCFG1) | 
	    SYSCTL_CLKCFG1_OTG_CLK_EN);
	tmp = rt305x_sysctl_get(SYSCTL_RSTCTRL);
	rt305x_sysctl_set(SYSCTL_RSTCTRL, tmp | SYSCTL_RSTCTRL_OTG);
	DELAY(100);
	/*
	 * Docs say that RSTCTRL bits for RT305x are W1C, so there should
	 * be no need for the below, but who really knows?
	 */
//	rt305x_sysctl_set(SYSCTL_RSTCTRL, tmp & ~SYSCTL_RSTCTRL_OTG);
//	DELAY(100);

	err = dwc_otg_init(sc);
	if (err) printf("dotg_init fail\n");
	if (!err) {
		err = device_probe_and_attach(sc->sc_bus.bdev);
		if (err) printf("device_probe_and_attach fail %d\n", err);
	}
	if (err) {
		goto error;
	}
	return (0);

error:
	dotg_obio_detach(dev);
	return (ENXIO);
}

static int
dotg_obio_detach(device_t dev)
{
	struct dwc_otg_softc *sc = device_get_softc(dev);
	int err;

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call dotg_obio_uninit() after dotg_obio_init()
		 */
		dwc_otg_uninit(sc);

		/* Stop OTG clock */
		rt305x_sysctl_set(SYSCTL_CLKCFG1, 
		    rt305x_sysctl_get(SYSCTL_CLKCFG1) & 
		    ~SYSCTL_CLKCFG1_OTG_CLK_EN);

		err = bus_teardown_intr(dev, sc->sc_irq_res,
		    sc->sc_intr_hdl);
		sc->sc_intr_hdl = NULL;
	}
	if (sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_bus, NULL);

	return (0);
}

static device_method_t dotg_obio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, dotg_obio_probe),
	DEVMETHOD(device_attach, dotg_obio_attach),
	DEVMETHOD(device_detach, dotg_obio_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

static driver_t dotg_obio_driver = {
	.name = "dwcotg",
	.methods = dotg_obio_methods,
	.size = sizeof(struct dwc_otg_softc),
};

static devclass_t dotg_obio_devclass;

DRIVER_MODULE(dwcotg, obio, dotg_obio_driver, dotg_obio_devclass, 0, 0);
