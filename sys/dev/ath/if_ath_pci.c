/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/ath/if_ath_pci.c,v 1.12 2005/03/05 19:06:12 imp Exp $");

/*
 * PCI/Cardbus front-end for the Atheros Wireless LAN controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <sys/socket.h>
 
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_arp.h>

#include <net80211/ieee80211_var.h>

#include <dev/ath/if_athvar.h>
#include <contrib/dev/ath/ah.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

/*
 * PCI glue.
 */

struct ath_pci_softc {
	struct ath_softc	sc_sc;
	struct resource		*sc_sr;		/* memory resource */
	struct resource		*sc_irq;	/* irq resource */
	void			*sc_ih;		/* interrupt handler */
};

#define	BS_BAR	0x10
#define	PCIR_RETRY_TIMEOUT	0x41

static int
ath_pci_probe(device_t dev)
{
	const char* devname;

	devname = ath_hal_probe(pci_get_vendor(dev), pci_get_device(dev));
	if (devname != NULL) {
		device_set_desc(dev, devname);
		return BUS_PROBE_DEFAULT;
	}
	return ENXIO;
}

static u_int32_t
ath_pci_setup(device_t dev)
{
	u_int32_t cmd;

	/*
	 * Enable memory mapping and bus mastering.
	 */
	cmd = pci_read_config(dev, PCIR_COMMAND, 4);
	cmd |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, cmd, 4);
	cmd = pci_read_config(dev, PCIR_COMMAND, 4);
	if ((cmd & PCIM_CMD_MEMEN) == 0) {
		device_printf(dev, "failed to enable memory mapping\n");
		return 0;
	}
	if ((cmd & PCIM_CMD_BUSMASTEREN) == 0) {
		device_printf(dev, "failed to enable bus mastering\n");
		return 0;
	}

	/*
	 * Disable retry timeout to keep PCI Tx retries from
	 * interfering with C3 CPU state.
	 */
	pci_write_config(dev, PCIR_RETRY_TIMEOUT, 0, 1);

	return 1;
}

static int
ath_pci_attach(device_t dev)
{
	struct ath_pci_softc *psc = device_get_softc(dev);
	struct ath_softc *sc = &psc->sc_sc;
	int error = ENXIO;
	int rid;

	sc->sc_dev = dev;

	if (!ath_pci_setup(dev))
		goto bad;

	/* 
	 * Setup memory-mapping of PCI registers.
	 */
	rid = BS_BAR;
	psc->sc_sr = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
					    RF_ACTIVE);
	if (psc->sc_sr == NULL) {
		device_printf(dev, "cannot map register space\n");
		goto bad;
	}
	sc->sc_st = rman_get_bustag(psc->sc_sr);
	sc->sc_sh = rman_get_bushandle(psc->sc_sr);
	/*
	 * Mark device invalid so any interrupts (shared or otherwise)
	 * that arrive before the HAL is setup are discarded.
	 */
	sc->sc_invalid = 1;

	/*
	 * Arrange interrupt line.
	 */
	rid = 0;
	psc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					     RF_SHAREABLE|RF_ACTIVE);
	if (psc->sc_irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		goto bad1;
	}
	if (bus_setup_intr(dev, psc->sc_irq,
			   INTR_TYPE_NET | INTR_MPSAFE,
			   ath_intr, sc, &psc->sc_ih)) {
		device_printf(dev, "could not establish interrupt\n");
		goto bad2;
	}

	/*
	 * Setup DMA descriptor area.
	 */
	if (bus_dma_tag_create(NULL,			/* parent */
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       0x3ffff,			/* maxsize XXX */
			       ATH_MAX_SCATTER,		/* nsegments */
			       BUS_SPACE_MAXADDR,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockarg */
			       &sc->sc_dmat)) {
		device_printf(dev, "cannot allocate DMA tag\n");
		goto bad3;
	}

	ATH_LOCK_INIT(sc);

	error = ath_attach(pci_get_device(dev), sc);
	if (error == 0)
		return error;

	ATH_LOCK_DESTROY(sc);
	bus_dma_tag_destroy(sc->sc_dmat);
bad3:
	bus_teardown_intr(dev, psc->sc_irq, psc->sc_ih);
bad2:
	bus_release_resource(dev, SYS_RES_IRQ, 0, psc->sc_irq);
bad1:
	bus_release_resource(dev, SYS_RES_MEMORY, BS_BAR, psc->sc_sr);
bad:
	return (error);
}

static int
ath_pci_detach(device_t dev)
{
	struct ath_pci_softc *psc = device_get_softc(dev);
	struct ath_softc *sc = &psc->sc_sc;

	/* check if device was removed */
	sc->sc_invalid = !bus_child_present(dev);

	ath_detach(sc);

	bus_generic_detach(dev);
	bus_teardown_intr(dev, psc->sc_irq, psc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, psc->sc_irq);

	bus_dma_tag_destroy(sc->sc_dmat);
	bus_release_resource(dev, SYS_RES_MEMORY, BS_BAR, psc->sc_sr);

	ATH_LOCK_DESTROY(sc);

	return (0);
}

static int
ath_pci_shutdown(device_t dev)
{
	struct ath_pci_softc *psc = device_get_softc(dev);

	ath_shutdown(&psc->sc_sc);
	return (0);
}

static int
ath_pci_suspend(device_t dev)
{
	struct ath_pci_softc *psc = device_get_softc(dev);

	ath_suspend(&psc->sc_sc);

	return (0);
}

static int
ath_pci_resume(device_t dev)
{
	struct ath_pci_softc *psc = device_get_softc(dev);

	if (!ath_pci_setup(dev))
		return ENXIO;

	ath_resume(&psc->sc_sc);

	return (0);
}

static device_method_t ath_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ath_pci_probe),
	DEVMETHOD(device_attach,	ath_pci_attach),
	DEVMETHOD(device_detach,	ath_pci_detach),
	DEVMETHOD(device_shutdown,	ath_pci_shutdown),
	DEVMETHOD(device_suspend,	ath_pci_suspend),
	DEVMETHOD(device_resume,	ath_pci_resume),

	{ 0,0 }
};
static driver_t ath_pci_driver = {
	"ath",
	ath_pci_methods,
	sizeof (struct ath_pci_softc)
};
static	devclass_t ath_devclass;
DRIVER_MODULE(if_ath, pci, ath_pci_driver, ath_devclass, 0, 0);
DRIVER_MODULE(if_ath, cardbus, ath_pci_driver, ath_devclass, 0, 0);
MODULE_VERSION(if_ath, 1);
MODULE_DEPEND(if_ath, ath_hal, 1, 1, 1);	/* Atheros HAL */
MODULE_DEPEND(if_ath, wlan, 1, 1, 1);		/* 802.11 media layer */
MODULE_DEPEND(if_ath, ath_rate, 1, 1, 1);	/* rate control algorithm */
