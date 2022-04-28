/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <powerpc/ofw/ofw_pci.h>

#include "pcib_if.h"

/*
 * Bus interface.
 */
static int		ofw_pci_read_ivar(device_t, device_t, int,
			    uintptr_t *);
static struct		resource * ofw_pci_alloc_resource(device_t bus,
			    device_t child, int type, int *rid, rman_res_t start,
			    rman_res_t end, rman_res_t count, u_int flags);
static int		ofw_pci_release_resource(device_t bus, device_t child,
    			    int type, int rid, struct resource *res);
static int		ofw_pci_activate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);
static int		ofw_pci_deactivate_resource(device_t bus,
    			    device_t child, int type, int rid,
    			    struct resource *res);
static int		ofw_pci_adjust_resource(device_t bus, device_t child,
			    int type, struct resource *res, rman_res_t start,
			    rman_res_t end);

/*
 * pcib interface.
 */
static int		ofw_pci_maxslots(device_t);
static int		ofw_pci_route_interrupt(device_t, device_t, int);

/*
 * ofw_bus interface
 */
static phandle_t ofw_pci_get_node(device_t bus, device_t dev);

/*
 * local methods
 */

static int ofw_pci_nranges(phandle_t node);
static int ofw_pci_fill_ranges(phandle_t node, struct ofw_pci_range *ranges);

/*
 * Driver methods.
 */
static device_method_t	ofw_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	ofw_pci_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	ofw_pci_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	ofw_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	ofw_pci_release_resource),
	DEVMETHOD(bus_activate_resource,	ofw_pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	ofw_pci_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,	ofw_pci_adjust_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	ofw_pci_maxslots),
	DEVMETHOD(pcib_route_interrupt,	ofw_pci_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,     ofw_pci_get_node),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ofw_pci, ofw_pci_driver, ofw_pci_methods, 0);

int
ofw_pci_init(device_t dev)
{
	struct		ofw_pci_softc *sc;
	phandle_t	node;
	u_int32_t	busrange[2];
	struct		ofw_pci_range *rp;
	int		error;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);
	sc->sc_initialized = 1;

	if (OF_getencprop(node, "bus-range", busrange, sizeof(busrange)) != 8)
		busrange[0] = 0;

	sc->sc_dev = dev;
	sc->sc_node = node;
	sc->sc_bus = busrange[0];

	if (sc->sc_quirks & OFW_PCI_QUIRK_RANGES_ON_CHILDREN) {
		phandle_t c;
		int n, i;
		
		sc->sc_nrange = 0;
		for (c = OF_child(node); c != 0; c = OF_peer(c)) {
			n = ofw_pci_nranges(c);
			if (n > 0)
				sc->sc_nrange += n;
		}
		if (sc->sc_nrange == 0)
			return (ENXIO);
		sc->sc_range = malloc(sc->sc_nrange * sizeof(sc->sc_range[0]),
		    M_DEVBUF, M_WAITOK);
		i = 0;
		for (c = OF_child(node); c != 0; c = OF_peer(c)) {
			n = ofw_pci_fill_ranges(c, &sc->sc_range[i]);
			if (n > 0)
				i += n;
		}
		KASSERT(i == sc->sc_nrange, ("range count mismatch"));
	} else {
		sc->sc_nrange = ofw_pci_nranges(node);
		if (sc->sc_nrange <= 0) {
			device_printf(dev, "could not get ranges\n");
			return (ENXIO);
		}
		sc->sc_range = malloc(sc->sc_nrange * sizeof(sc->sc_range[0]),
		    M_DEVBUF, M_WAITOK);
		ofw_pci_fill_ranges(node, sc->sc_range);
	}
		
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "PCI I/O Ports";
	error = rman_init(&sc->sc_io_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "PCI Memory";
	error = rman_init(&sc->sc_mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	for (rp = sc->sc_range; rp < sc->sc_range + sc->sc_nrange &&
	       rp->pci_hi != 0; rp++) {
		error = 0;

		switch (rp->pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_CONFIG:
			break;
		case OFW_PCI_PHYS_HI_SPACE_IO:
			error = rman_manage_region(&sc->sc_io_rman, rp->pci,
			    rp->pci + rp->size - 1);
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM32:
		case OFW_PCI_PHYS_HI_SPACE_MEM64:
			error = rman_manage_region(&sc->sc_mem_rman, rp->pci,
			    rp->pci + rp->size - 1);
			break;
		}

		if (error) {
			device_printf(dev, 
			    "rman_manage_region(%x, %#jx, %#jx) failed. "
			    "error = %d\n", rp->pci_hi &
			    OFW_PCI_PHYS_HI_SPACEMASK, rp->pci,
			    rp->pci + rp->size - 1, error);
			return (error);
		}
	}

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(cell_t));

	return (error);
}

int
ofw_pci_attach(device_t dev)
{
	struct ofw_pci_softc *sc;
	int error;

	sc = device_get_softc(dev);
	if (!sc->sc_initialized) {
		error = ofw_pci_init(dev);
		if (error)
			return (error);
	}

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static int
ofw_pci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static int
ofw_pci_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct ofw_pci_softc *sc;
	struct ofw_pci_register reg;
	uint32_t pintr, mintr[2];
	int intrcells;
	phandle_t iparent;

	sc = device_get_softc(bus);
	pintr = pin;

	/* Fabricate imap information in case this isn't an OFW device */
	bzero(&reg, sizeof(reg));
	reg.phys_hi = (pci_get_bus(dev) << OFW_PCI_PHYS_HI_BUSSHIFT) |
	    (pci_get_slot(dev) << OFW_PCI_PHYS_HI_DEVICESHIFT) |
	    (pci_get_function(dev) << OFW_PCI_PHYS_HI_FUNCTIONSHIFT);

	intrcells = ofw_bus_lookup_imap(ofw_bus_get_node(dev),
	    &sc->sc_pci_iinfo, &reg, sizeof(reg), &pintr, sizeof(pintr),
	    mintr, sizeof(mintr), &iparent);
	if (intrcells) {
		pintr = ofw_bus_map_intr(dev, iparent, intrcells, mintr);
		return (pintr);
	}

	/* Maybe it's a real interrupt, not an intpin */
	if (pin > 4)
		return (pin);

	device_printf(bus, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
}

static int
ofw_pci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	ofw_pci_softc *sc;

	sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
	}

	return (ENOENT);
}

static struct resource *
ofw_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct			ofw_pci_softc *sc;
	struct			resource *rv;
	struct			rman *rm;
	int			needactivate;

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);

	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;

	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		break;

	case SYS_RES_IRQ:
		return (bus_alloc_resource(bus, type, rid, start, end, count,
		    flags));

	default:
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL) {
		device_printf(bus, "failed to reserve resource for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv) != 0) {
			device_printf(bus,
			    "failed to activate resource for %s\n",
			    device_get_nameunit(child));
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
ofw_pci_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	if (rman_get_flags(res) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, res);
		if (error)
			return error;
	}

	return (rman_release_resource(res));
}

static int
ofw_pci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	struct ofw_pci_softc *sc;
	void	*p;

	sc = device_get_softc(bus);

	if (type == SYS_RES_IRQ) {
		return (bus_activate_resource(bus, type, rid, res));
	}
	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		struct ofw_pci_range *rp;
		vm_paddr_t start;
		int space;

		start = (vm_paddr_t)rman_get_start(res);

		/*
		 * Map this through the ranges list
		 */
		for (rp = sc->sc_range; rp < sc->sc_range + sc->sc_nrange &&
		       rp->pci_hi != 0; rp++) {
			if (start < rp->pci || start >= rp->pci + rp->size)
				continue;

			switch (rp->pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
			case OFW_PCI_PHYS_HI_SPACE_IO:
				space = SYS_RES_IOPORT;
				break;
			case OFW_PCI_PHYS_HI_SPACE_MEM32:
			case OFW_PCI_PHYS_HI_SPACE_MEM64:
				space = SYS_RES_MEMORY;
				break;
			default:
				space = -1;
			}

			if (type == space) {
				start += (rp->host - rp->pci);
				break;
			}
		}

		if (bootverbose)
			printf("ofw_pci mapdev: start %jx, len %jd\n",
			    (rman_res_t)start, rman_get_size(res));

		p = pmap_mapdev(start, (vm_size_t)rman_get_size(res));
		if (p == NULL)
			return (ENOMEM);

		rman_set_virtual(res, p);
		rman_set_bustag(res, &bs_le_tag);
		rman_set_bushandle(res, (u_long)p);
	}

	return (rman_activate_resource(res));
}

static int
ofw_pci_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		u_int32_t psize;

		psize = rman_get_size(res);
		pmap_unmapdev((vm_offset_t)rman_get_virtual(res), psize);
	}

	return (rman_deactivate_resource(res));
}

static int
ofw_pci_adjust_resource(device_t bus, device_t child, int type,
    struct resource *res, rman_res_t start, rman_res_t end)
{
	struct rman *rm = NULL;
	struct ofw_pci_softc *sc = device_get_softc(bus);

	KASSERT(!(rman_get_flags(res) & RF_ACTIVE),
	    ("active resources cannot be adjusted"));
	if (rman_get_flags(res) & RF_ACTIVE)
		return (EINVAL);

	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		break;
	default:
		return (ENXIO);
	}

	if (!rman_is_region_manager(res, rm))
		return (EINVAL);

	return (rman_adjust_resource(res, start, end));
}
	

static phandle_t
ofw_pci_get_node(device_t bus, device_t dev)
{
	struct ofw_pci_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */

	return (sc->sc_node);
}

static int
ofw_pci_nranges(phandle_t node)
{
	int host_address_cells = 1, pci_address_cells = 3, size_cells = 2;
	ssize_t nbase_ranges;

	OF_getencprop(OF_parent(node), "#address-cells", &host_address_cells,
	    sizeof(host_address_cells));
	OF_getencprop(node, "#address-cells", &pci_address_cells,
	    sizeof(pci_address_cells));
	OF_getencprop(node, "#size-cells", &size_cells, sizeof(size_cells));

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges <= 0)
		return (-1);

	return (nbase_ranges / sizeof(cell_t) /
	    (pci_address_cells + host_address_cells + size_cells));
}

static int
ofw_pci_fill_ranges(phandle_t node, struct ofw_pci_range *ranges)
{
	int host_address_cells = 1, pci_address_cells = 3, size_cells = 2;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int nranges;
	int i, j, k;

	OF_getencprop(OF_parent(node), "#address-cells", &host_address_cells,
	    sizeof(host_address_cells));
	OF_getencprop(node, "#address-cells", &pci_address_cells,
	    sizeof(pci_address_cells));
	OF_getencprop(node, "#size-cells", &size_cells, sizeof(size_cells));

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges <= 0)
		return (-1);
	nranges = nbase_ranges / sizeof(cell_t) /
	    (pci_address_cells + host_address_cells + size_cells);

	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < nranges; i++) {
		ranges[i].pci_hi = base_ranges[j++];
		ranges[i].pci = 0;
		for (k = 0; k < pci_address_cells - 1; k++) {
			ranges[i].pci <<= 32;
			ranges[i].pci |= base_ranges[j++];
		}
		ranges[i].host = 0;
		for (k = 0; k < host_address_cells; k++) {
			ranges[i].host <<= 32;
			ranges[i].host |= base_ranges[j++];
		}
		ranges[i].size = 0;
		for (k = 0; k < size_cells; k++) {
			ranges[i].size <<= 32;
			ranges[i].size |= base_ranges[j++];
		}
	}

	free(base_ranges, M_DEVBUF);
	return (nranges);
}

