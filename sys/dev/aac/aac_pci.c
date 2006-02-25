/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2001 Scott Long
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 Adaptec, Inc.
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
__FBSDID("$FreeBSD: src/sys/dev/aac/aac_pci.c,v 1.54.2.3 2005/10/09 06:39:21 scottl Exp $");

/*
 * PCI bus interface and resource allocation.
 */

#include "opt_aac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/aac/aacreg.h>
#include <sys/aac_ioctl.h>
#include <dev/aac/aacvar.h>

static int	aac_pci_probe(device_t dev);
static int	aac_pci_attach(device_t dev);

static device_method_t aac_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aac_pci_probe),
	DEVMETHOD(device_attach,	aac_pci_attach),
	DEVMETHOD(device_detach,	aac_detach),
	DEVMETHOD(device_suspend,	aac_suspend),
	DEVMETHOD(device_resume,	aac_resume),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	{ 0, 0 }
};

static driver_t aac_pci_driver = {
	"aac",
	aac_methods,
	sizeof(struct aac_softc)
};

static devclass_t	aac_devclass;

DRIVER_MODULE(aac, pci, aac_pci_driver, aac_devclass, 0, 0);

struct aac_ident
{
	u_int16_t		vendor;
	u_int16_t		device;
	u_int16_t		subvendor;
	u_int16_t		subdevice;
	int			hwif;
	int			quirks;
	char			*desc;
} aac_identifiers[] = {
	{0x1028, 0x0001, 0x1028, 0x0001, AAC_HWIF_I960RX, 0,
	"Dell PERC 2/Si"},
	{0x1028, 0x0002, 0x1028, 0x0002, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Di"},
	{0x1028, 0x0003, 0x1028, 0x0003, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Si"},
	{0x1028, 0x0004, 0x1028, 0x00d0, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Si"},
	{0x1028, 0x0002, 0x1028, 0x00d1, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Di"},
	{0x1028, 0x0002, 0x1028, 0x00d9, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Di"},
	{0x1028, 0x000a, 0x1028, 0x0106, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Di"},
	{0x1028, 0x000a, 0x1028, 0x011b, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Di"},
	{0x1028, 0x000a, 0x1028, 0x0121, AAC_HWIF_I960RX, 0,
	"Dell PERC 3/Di"},
	{0x1011, 0x0046, 0x9005, 0x0364, AAC_HWIF_STRONGARM, 0,
	"Adaptec AAC-364"},
	{0x1011, 0x0046, 0x9005, 0x0365, AAC_HWIF_STRONGARM,
	 AAC_FLAGS_BROKEN_MEMMAP, "Adaptec SCSI RAID 5400S"},
	{0x1011, 0x0046, 0x9005, 0x1364, AAC_HWIF_STRONGARM, AAC_FLAGS_PERC2QC,
	 "Dell PERC 2/QC"},
	{0x1011, 0x0046, 0x103c, 0x10c2, AAC_HWIF_STRONGARM, 0,
	 "HP NetRaid-4M"},
	{0x9005, 0x0285, 0x9005, 0x0285, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB |
	 AAC_FLAGS_256FIBS, "Adaptec SCSI RAID 2200S"},
	{0x9005, 0x0285, 0x1028, 0x0287, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB |
	 AAC_FLAGS_256FIBS, "Dell PERC 320/DC"},
	{0x9005, 0x0285, 0x9005, 0x0286, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB |
	 AAC_FLAGS_256FIBS, "Adaptec SCSI RAID 2120S"},
	{0x9005, 0x0285, 0x9005, 0x0290, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB,
	 "Adaptec SCSI RAID 2410SA"},
	{0x9005, 0x0285, 0x1028, 0x0291, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB,
	 "Dell CERC SATA RAID 2"},
	{0x9005, 0x0285, 0x9005, 0x0292, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB,
	 "Adaptec SCSI RAID 2810SA"},
	{0x9005, 0x0285, 0x9005, 0x0293, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB,
	 "Adaptec SCSI RAID 21610SA"},
	{0x9005, 0x0285, 0x103c, 0x3227, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB,
	 "HP ML110 G2 (Adaptec 2610SA)"},
	{0x9005, 0x0286, 0x9005, 0x028c, AAC_HWIF_RKT, 0,
	 "Adaptec SCSI RAID 2230S"},
	{0x9005, 0x0286, 0x9005, 0x028d, AAC_HWIF_RKT, 0,
	 "Adaptec SCSI RAID 2130S"},

	{0x9005, 0x0285, 0x9005, 0x0287, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB |
	 AAC_FLAGS_256FIBS, "Adaptec SCSI RAID 2200S"},
	{0x9005, 0x0285, 0x17aa, 0x0286, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB |
	 AAC_FLAGS_256FIBS, "Legend S220"},
	{0x9005, 0x0285, 0x17aa, 0x0287, AAC_HWIF_I960RX, AAC_FLAGS_NO4GB |
	 AAC_FLAGS_256FIBS, "Legend S230"},
	{0x9005, 0x0285, 0x9005, 0x0288, AAC_HWIF_I960RX, 0,
	 "Adaptec SCSI RAID 3230S"},
	{0x9005, 0x0285, 0x9005, 0x0289, AAC_HWIF_I960RX, 0,
	 "Adaptec SCSI RAID 3240S"},
	{0x9005, 0x0285, 0x9005, 0x028a, AAC_HWIF_I960RX, 0,
	 "Adaptec SCSI RAID 2020ZCR"},
	{0x9005, 0x0285, 0x9005, 0x028b, AAC_HWIF_I960RX, 0,
	 "Adaptec SCSI RAID 2025ZCR"},
	{0x9005, 0x0286, 0x9005, 0x029b, AAC_HWIF_RKT, 0,
	 "Adaptec SATA RAID 2820SA"},
	{0x9005, 0x0286, 0x9005, 0x029c, AAC_HWIF_RKT, 0,
	 "Adaptec SATA RAID 2620SA"},
	{0x9005, 0x0286, 0x9005, 0x029d, AAC_HWIF_RKT, 0,
	 "Adaptec SATA RAID 2420SA"},
	{0x9005, 0x0286, 0x9005, 0x029e, AAC_HWIF_RKT, 0,
	 "ICP9024RO SATA RAID"},
	{0x9005, 0x0286, 0x9005, 0x029f, AAC_HWIF_RKT, 0,
	 "ICP9014RO SATA RAID"},
	{0x9005, 0x0285, 0x9005, 0x0294, AAC_HWIF_I960RX, 0,
	 "Adaptec SATA RAID 2026ZCR"},
	{0x9005, 0x0285, 0x103c, 0x3227, AAC_HWIF_I960RX, 0,
	 "Adaptec SATA RAID 2610SA"},
	{0x9005, 0x0285, 0x9005, 0x0296, AAC_HWIF_I960RX, 0,
	 "Adaptec SCSI RAID 2240S"},
	{0x9005, 0x0285, 0x9005, 0x0297, AAC_HWIF_I960RX, 0,
	 "Adaptec SAS RAID 4005SAS"},
	{0x9005, 0x0285, 0x1014, 0x02f2, AAC_HWIF_I960RX, 0,
	 "IBM ServeRAID 8i"},
	{0x9005, 0x0285, 0x9005, 0x0298, AAC_HWIF_I960RX, 0,
	 "Adaptec SAS RAID 4000SAS"},
	{0x9005, 0x0285, 0x9005, 0x0299, AAC_HWIF_I960RX, 0,
	 "Adaptec SAS RAID 4800SAS"},
	{0x9005, 0x0285, 0x9005, 0x029a, AAC_HWIF_I960RX, 0,
	 "Adaptec SAS RAID 4805SAS"},
	{0x9005, 0x0285, 0x9005, 0x028e, AAC_HWIF_I960RX, 0,
	 "Adaptec SATA RAID 2020SA ZCR"},
	{0x9005, 0x0285, 0x9005, 0x028f, AAC_HWIF_I960RX, 0,
	 "Adaptec SATA RAID 2025SA ZCR"},
	{0x9005, 0x0285, 0x9005, 0x02a4, AAC_HWIF_I960RX, 0,
	 "ICP 9085LI SAS RAID"},
	{0x9005, 0x0285, 0x9005, 0x02a5, AAC_HWIF_I960RX, 0,
	 "ICP 5085BR SAS RAID"},
	{0x9005, 0x0286, 0x9005, 0x02a0, AAC_HWIF_RKT, 0,
	 "ICP9047MA SATA RAID"},
	{0x9005, 0x0286, 0x9005, 0x02a1, AAC_HWIF_RKT, 0,
	 "ICP9087MA SATA RAID"},
	{0, 0, 0, 0, 0, 0, 0}
};

/*
 * Determine whether this is one of our supported adapters.
 */
static int
aac_pci_probe(device_t dev)
{
	struct aac_ident *m;

	debug_called(1);

	for (m = aac_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == pci_get_vendor(dev)) &&
		    (m->device == pci_get_device(dev)) &&
		    ((m->subvendor == 0) || (m->subvendor ==
					     pci_get_subvendor(dev))) &&
		    ((m->subdevice == 0) || ((m->subdevice ==
					      pci_get_subdevice(dev))))) {
		
			device_set_desc(dev, m->desc);
			return(BUS_PROBE_DEFAULT);
		}
	}
	return(ENXIO);
}

/*
 * Allocate resources for our device, set up the bus interface.
 */
static int
aac_pci_attach(device_t dev)
{
	struct aac_softc *sc;
	int i, error;
	u_int32_t command;

	debug_called(1);

	/*
	 * Initialise softc.
	 */
	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->aac_dev = dev;

	/* assume failure is 'not configured' */
	error = ENXIO;

	/* 
	 * Verify that the adapter is correctly set up in PCI space.
	 */
	command = pci_read_config(sc->aac_dev, PCIR_COMMAND, 2);
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 2);
	command = pci_read_config(sc->aac_dev, PCIR_COMMAND, 2);
	if (!(command & PCIM_CMD_BUSMASTEREN)) {
		device_printf(sc->aac_dev, "can't enable bus-master feature\n");
		goto out;
	}
	if ((command & PCIM_CMD_MEMEN) == 0) {
		device_printf(sc->aac_dev, "memory window not available\n");
		goto out;
	}

	/*
	 * Allocate the PCI register window.
	 */
	sc->aac_regs_rid = PCIR_BAR(0);
	if ((sc->aac_regs_resource = bus_alloc_resource_any(sc->aac_dev,
							    SYS_RES_MEMORY,
							    &sc->aac_regs_rid,
							    RF_ACTIVE)) ==
							    NULL) {
		device_printf(sc->aac_dev,
			      "couldn't allocate register window\n");
		goto out;
	}
	sc->aac_btag = rman_get_bustag(sc->aac_regs_resource);
	sc->aac_bhandle = rman_get_bushandle(sc->aac_regs_resource);

	/* assume failure is 'out of memory' */
	error = ENOMEM;

	/*
	 * Allocate the parent bus DMA tag appropriate for our PCI interface.
	 * 
	 * Note that some of these controllers are 64-bit capable.
	 */
	if (bus_dma_tag_create(NULL, 			/* parent */
			       PAGE_SIZE, 0,		/* algnmnt, boundary */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR, 	/* highaddr */
			       NULL, NULL, 		/* filter, filterarg */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
			       BUS_SPACE_UNRESTRICTED,	/* nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       0,			/* flags */
			       NULL, NULL,		/* No locking needed */
			       &sc->aac_parent_dmat)) {
		device_printf(sc->aac_dev, "can't allocate parent DMA tag\n");
		goto out;
	}

	/* 
	 * Detect the hardware interface version, set up the bus interface
	 * indirection.
	 */
	for (i = 0; aac_identifiers[i].vendor != 0; i++) {
		if ((aac_identifiers[i].vendor == pci_get_vendor(dev)) &&
		    (aac_identifiers[i].device == pci_get_device(dev)) &&
		    (aac_identifiers[i].subvendor == pci_get_subvendor(dev)) &&
		    (aac_identifiers[i].subdevice == pci_get_subdevice(dev))) {
			sc->aac_hwif = aac_identifiers[i].hwif;
			switch(sc->aac_hwif) {
			case AAC_HWIF_I960RX:
				debug(2, "set hardware up for i960Rx");
				sc->aac_if = aac_rx_interface;
				break;
			case AAC_HWIF_STRONGARM:
				debug(2, "set hardware up for StrongARM");
				sc->aac_if = aac_sa_interface;
				break;
			case AAC_HWIF_FALCON:
				debug(2, "set hardware up for Falcon/PPC");
				sc->aac_if = aac_fa_interface;
				break;
			case AAC_HWIF_RKT:
				debug(2, "set hardware up for Rocket/MIPS");
				sc->aac_if = aac_rkt_interface;
				break;
			default:
				sc->aac_hwif = AAC_HWIF_UNKNOWN;
				break;
			}

			/* Set up quirks */
			sc->flags = aac_identifiers[i].quirks;

			break;
		}
	}
	if (sc->aac_hwif == AAC_HWIF_UNKNOWN) {
		device_printf(sc->aac_dev, "unknown hardware type\n");
		error = ENXIO;
		goto out;
	}


	/*
	 * Do bus-independent initialisation.
	 */
	error = aac_attach(sc);

out:
	if (error)
		aac_free(sc);
	return(error);
}

/*
 * Do nothing driver that will attach to the SCSI channels of a Dell PERC
 * controller.  This is needed to keep the power management subsystem from
 * trying to power down these devices.
 */
static int aacch_probe(device_t dev);
static int aacch_attach(device_t dev);
static int aacch_detach(device_t dev);

static device_method_t aacch_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aacch_probe),
	DEVMETHOD(device_attach,	aacch_attach),
	DEVMETHOD(device_detach,	aacch_detach),
	{ 0, 0 }
};

struct aacch_softc {
	device_t	dev;
};

static driver_t aacch_driver = {
	"aacch",
	aacch_methods,
	sizeof(struct aacch_softc)
};

static devclass_t	aacch_devclass;
DRIVER_MODULE(aacch, pci, aacch_driver, aacch_devclass, 0, 0);

static int
aacch_probe(device_t dev)
{

	if ((pci_get_vendor(dev) != 0x9005) ||
	    (pci_get_device(dev) != 0x00c5))
		return (ENXIO);

	device_set_desc(dev, "AAC RAID Channel");
	return (-10);
}

static int
aacch_attach(device_t dev)
{
	struct aacch_softc *sc;

	sc = device_get_softc(dev);

	sc->dev = dev;

	return (0);
}

static int
aacch_detach(device_t dev)
{

	return (0);
}

