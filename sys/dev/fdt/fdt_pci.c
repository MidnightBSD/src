/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/fdt/fdt_common.h>
#include <dev/pci/pcireg.h>

#include <machine/fdt.h>

#include "ofw_bus_if.h"
#include "pcib_if.h"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

#define FDT_RANGES_CELLS	((3 + 3 + 2) * 2)

static void
fdt_pci_range_dump(struct fdt_pci_range *range)
{
#ifdef DEBUG
	printf("\n");
	printf("  base_pci = 0x%08lx\n", range->base_pci);
	printf("  base_par = 0x%08lx\n", range->base_parent);
	printf("  len      = 0x%08lx\n", range->len);
#endif
}

int
fdt_pci_ranges_decode(phandle_t node, struct fdt_pci_range *io_space,
    struct fdt_pci_range *mem_space)
{
	pcell_t ranges[FDT_RANGES_CELLS];
	struct fdt_pci_range *pci_space;
	pcell_t addr_cells, size_cells, par_addr_cells;
	pcell_t *rangesptr;
	pcell_t cell0, cell1, cell2;
	int tuple_size, tuples, i, rv, offset_cells, len;

	/*
	 * Retrieve 'ranges' property.
	 */
	if ((fdt_addrsize_cells(node, &addr_cells, &size_cells)) != 0)
		return (EINVAL);
	if (addr_cells != 3 || size_cells != 2)
		return (ERANGE);

	par_addr_cells = fdt_parent_addr_cells(node);
	if (par_addr_cells > 3)
		return (ERANGE);

	len = OF_getproplen(node, "ranges");
	if (len > sizeof(ranges))
		return (ENOMEM);

	if (OF_getprop(node, "ranges", ranges, sizeof(ranges)) <= 0)
		return (EINVAL);

	tuple_size = sizeof(pcell_t) * (addr_cells + par_addr_cells +
	    size_cells);
	tuples = len / tuple_size;

	rangesptr = &ranges[0];
	offset_cells = 0;
	for (i = 0; i < tuples; i++) {
		cell0 = fdt_data_get((void *)rangesptr, 1);
		rangesptr++;
		cell1 = fdt_data_get((void *)rangesptr, 1);
		rangesptr++;
		cell2 = fdt_data_get((void *)rangesptr, 1);
		rangesptr++;

		if (cell0 & 0x02000000) {
			pci_space = mem_space;
		} else if (cell0 & 0x01000000) {
			pci_space = io_space;
		} else {
			rv = ERANGE;
			goto out;
		}

		if (par_addr_cells == 3) {
			/*
			 * This is a PCI subnode 'ranges'. Skip cell0 and
			 * cell1 of this entry and only use cell2.
			 */
			offset_cells = 2;
			rangesptr += offset_cells;
		}

		if (fdt_data_verify((void *)rangesptr, par_addr_cells -
		    offset_cells)) {
			rv = ERANGE;
			goto out;
		}
		pci_space->base_parent = fdt_data_get((void *)rangesptr,
		    par_addr_cells - offset_cells);
		rangesptr += par_addr_cells - offset_cells;

		if (fdt_data_verify((void *)rangesptr, size_cells)) {
			rv = ERANGE;
			goto out;
		}
		pci_space->len = fdt_data_get((void *)rangesptr, size_cells);
		rangesptr += size_cells;

		pci_space->base_pci = cell2;
	}
	rv = 0;
out:
	return (rv);
}

int
fdt_pci_ranges(phandle_t node, struct fdt_pci_range *io_space,
    struct fdt_pci_range *mem_space)
{
	int err;

	debugf("Processing PCI node: %x\n", node);
	if ((err = fdt_pci_ranges_decode(node, io_space, mem_space)) != 0) {
		debugf("could not decode parent PCI node 'ranges'\n");
		return (err);
	}

	debugf("Post fixup dump:\n");
	fdt_pci_range_dump(io_space);
	fdt_pci_range_dump(mem_space);
	return (0);
}

static int
fdt_addr_cells(phandle_t node, int *addr_cells)
{
	pcell_t cell;
	int cell_size;

	cell_size = sizeof(cell);
	if (OF_getprop(node, "#address-cells", &cell, cell_size) < cell_size)
		return (EINVAL);
	*addr_cells = fdt32_to_cpu((int)cell);

	if (*addr_cells > 3)
		return (ERANGE);
	return (0);
}

static int
fdt_interrupt_cells(phandle_t node)
{
	pcell_t intr_cells;

	if (OF_getprop(node, "#interrupt-cells", &intr_cells,
	    sizeof(intr_cells)) <= 0) {
		debugf("no intr-cells defined, defaulting to 1\n");
		intr_cells = 1;
	}
	intr_cells = fdt32_to_cpu(intr_cells);

	return ((int)intr_cells);
}

int
fdt_pci_intr_info(phandle_t node, struct fdt_pci_intr *intr_info)
{
	void *map, *mask;
	int acells, icells;
	int error, len;

	error = fdt_addr_cells(node, &acells);
	if (error)
		return (error);

	icells = fdt_interrupt_cells(node);

	/*
	 * Retrieve the interrupt map and mask properties.
	 */
	len = OF_getprop_alloc(node, "interrupt-map-mask", 1, &mask);
	if (len / sizeof(pcell_t) != (acells + icells)) {
		debugf("bad mask len = %d\n", len);
		goto err;
	}

	len = OF_getprop_alloc(node, "interrupt-map", 1, &map);
	if (len <= 0) {
		debugf("bad map len = %d\n", len);
		goto err;
	}

	intr_info->map_len = len;
	intr_info->map = map;
	intr_info->mask = mask;
	intr_info->addr_cells = acells;
	intr_info->intr_cells = icells;

	debugf("acells=%u, icells=%u, map_len=%u\n", acells, icells, len);
	return (0);

err:
	free(mask, M_OFWPROP);
	return (ENXIO);
}

int
fdt_pci_route_intr(int bus, int slot, int func, int pin,
    struct fdt_pci_intr *intr_info, int *interrupt)
{
	pcell_t child_spec[4], masked[4];
	ihandle_t iph;
	pcell_t intr_par;
	pcell_t *map_ptr;
	uint32_t addr;
	int i, j, map_len;
	int par_intr_cells, par_addr_cells, child_spec_cells, row_cells;
	int par_idx, spec_idx, err, trig, pol;

	child_spec_cells = intr_info->addr_cells + intr_info->intr_cells;
	if (child_spec_cells > sizeof(child_spec) / sizeof(pcell_t))
		return (ENOMEM);

	addr = (bus << 16) | (slot << 11) | (func << 8);
	child_spec[0] = addr;
	child_spec[1] = 0;
	child_spec[2] = 0;
	child_spec[3] = pin;

	map_len = intr_info->map_len;
	map_ptr = intr_info->map;

	par_idx = child_spec_cells;
	i = 0;
	while (i < map_len) {
		iph = fdt32_to_cpu(map_ptr[par_idx]);
		intr_par = OF_instance_to_package(iph);

		err = fdt_addr_cells(intr_par, &par_addr_cells);
		if (err != 0) {
			debugf("could not retrieve intr parent #addr-cells\n");
			return (err);
		}
		par_intr_cells = fdt_interrupt_cells(intr_par);

		row_cells = child_spec_cells + 1 + par_addr_cells +
		    par_intr_cells;

		/*
		 * Apply mask and look up the entry in interrupt map.
		 */
		for (j = 0; j < child_spec_cells; j++) {
			masked[j] = child_spec[j] &
			    fdt32_to_cpu(intr_info->mask[j]);

			if (masked[j] != fdt32_to_cpu(map_ptr[j]))
				goto next;
		}

		/*
		 * Decode interrupt of the parent intr controller.
		 */
		spec_idx = child_spec_cells + 1 + par_addr_cells;
		err = fdt_intr_decode(intr_par, &map_ptr[spec_idx],
		    interrupt, &trig, &pol);
		if (err != 0) {
			debugf("could not decode interrupt\n");
			return (err);
		}
		debugf("decoded intr = %d, trig = %d, pol = %d\n", *interrupt,
		    trig, pol);

#if defined(__powerpc__)
		powerpc_config_intr(FDT_MAP_IRQ(intr_par, *interrupt), trig,
		    pol);
#endif
		return (0);

next:
		map_ptr += row_cells;
		i += (row_cells * sizeof(pcell_t));
	}

	return (ENXIO);
}

#if defined(__arm__)
int
fdt_pci_devmap(phandle_t node, struct pmap_devmap *devmap, vm_offset_t io_va,
    vm_offset_t mem_va)
{
	struct fdt_pci_range io_space, mem_space;
	int error;

	if ((error = fdt_pci_ranges_decode(node, &io_space, &mem_space)) != 0)
		return (error);

	devmap->pd_va = io_va;
	devmap->pd_pa = io_space.base_parent;
	devmap->pd_size = io_space.len;
	devmap->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	devmap->pd_cache = PTE_NOCACHE;
	devmap++;

	devmap->pd_va = mem_va;
	devmap->pd_pa = mem_space.base_parent;
	devmap->pd_size = mem_space.len;
	devmap->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	devmap->pd_cache = PTE_NOCACHE;
	return (0);
}
#endif

#if 0
static int
fdt_pci_config_bar(device_t dev, int bus, int slot, int func, int bar)
{
}

static int
fdt_pci_config_normal(device_t dev, int bus, int slot, int func)
{
	int bar;
	uint8_t command, intline, intpin;

	command = PCIB_READ_CONFIG(dev, bus, slot, func, PCIR_COMMAND, 1);
	command &= ~(PCIM_CMD_MEMEN | PCIM_CMD_PORTEN);
	PCIB_WRITE_CONFIG(dev, bus, slot, func, PCIR_COMMAND, command, 1);

	/* Program the base address registers. */
	bar = 0;
	while (bar <= PCIR_MAX_BAR_0)
		bar += fdt_pci_config_bar(dev, bus, slot, func, bar);

	/* Perform interrupt routing. */
	intpin = PCIB_READ_CONFIG(dev, bus, slot, func, PCIR_INTPIN, 1);
	intline = fsl_pcib_route_int(dev, bus, slot, func, intpin);
	PCIB_WRITE_CONFIG(dev, bus, slot, func, PCIR_INTLINE, intline, 1);

	command |= PCIM_CMD_MEMEN | PCIM_CMD_PORTEN;
	PCIB_WRITE_CONFIG(dev, bus, slot, func, PCIR_COMMAND, command, 1);
}

static int
fdt_pci_config_bridge(device_t dev, int bus, int secbus, int slot, int func)
{
	int maxbar;
	uint8_t command;

	command = PCIB_READ_CONFIG(dev, bus, slot, func, PCIR_COMMAND, 1);
	command &= ~(PCIM_CMD_MEMEN | PCIM_CMD_PORTEN);
	PCIB_WRITE_CONFIG(dev, bus, slot, func, PCIR_COMMAND, command, 1);

	/* Program the base address registers. */
                        maxbar = (hdrtype & PCIM_HDRTYPE) ? 1 : 6;
                        bar = 0;
                        while (bar < maxbar)
                                bar += fsl_pcib_init_bar(sc, bus, slot, func,
                                    bar);

                        /* Perform interrupt routing. */
                        intpin = fsl_pcib_read_config(sc->sc_dev, bus, slot,
                            func, PCIR_INTPIN, 1);
                        intline = fsl_pcib_route_int(sc, bus, slot, func,
                            intpin);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_INTLINE, intline, 1);

                        command |= PCIM_CMD_MEMEN | PCIM_CMD_PORTEN;
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_COMMAND, command, 1);

                        /*
                         * Handle PCI-PCI bridges
                         */
                        class = fsl_pcib_read_config(sc->sc_dev, bus, slot,
                            func, PCIR_CLASS, 1);
                        subclass = fsl_pcib_read_config(sc->sc_dev, bus, slot,
                            func, PCIR_SUBCLASS, 1);

                        /* Allow only proper PCI-PCI briges */
                        if (class != PCIC_BRIDGE)
                                continue;
                        if (subclass != PCIS_BRIDGE_PCI)
                                continue;

                        secbus++;

                        /* Program I/O decoder. */
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_IOBASEL_1, sc->sc_ioport.rm_start >> 8, 1);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_IOLIMITL_1, sc->sc_ioport.rm_end >> 8, 1);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_IOBASEH_1, sc->sc_ioport.rm_start >> 16, 2);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_IOLIMITH_1, sc->sc_ioport.rm_end >> 16, 2);

                        /* Program (non-prefetchable) memory decoder. */
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_MEMBASE_1, sc->sc_iomem.rm_start >> 16, 2);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_MEMLIMIT_1, sc->sc_iomem.rm_end >> 16, 2);

                        /* Program prefetchable memory decoder. */
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_PMBASEL_1, 0x0010, 2);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_PMLIMITL_1, 0x000f, 2);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_PMBASEH_1, 0x00000000, 4);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_PMLIMITH_1, 0x00000000, 4);

                        /* Read currect bus register configuration */
                        old_pribus = fsl_pcib_read_config(sc->sc_dev, bus,
                            slot, func, PCIR_PRIBUS_1, 1);
                        old_secbus = fsl_pcib_read_config(sc->sc_dev, bus,
                            slot, func, PCIR_SECBUS_1, 1);
                        old_subbus = fsl_pcib_read_config(sc->sc_dev, bus,
                            slot, func, PCIR_SUBBUS_1, 1);

                        if (bootverbose)
                                printf("PCI: reading firmware bus numbers for "
                                    "secbus = %d (bus/sec/sub) = (%d/%d/%d)\n",
                                    secbus, old_pribus, old_secbus, old_subbus);

                        new_pribus = bus;
                        new_secbus = secbus;

                        secbus = fsl_pcib_init(sc, secbus,
                            (subclass == PCIS_BRIDGE_PCI) ? PCI_SLOTMAX : 0);

                        new_subbus = secbus;

                        if (bootverbose)
                                printf("PCI: translate firmware bus numbers "
                                    "for secbus %d (%d/%d/%d) -> (%d/%d/%d)\n",
                                    secbus, old_pribus, old_secbus, old_subbus,
                                    new_pribus, new_secbus, new_subbus);

                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_PRIBUS_1, new_pribus, 1);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_SECBUS_1, new_secbus, 1);
                        fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
                            PCIR_SUBBUS_1, new_subbus, 1);

}

static int
fdt_pci_config_slot(device_t dev, int bus, int secbus, int slot)
{
	int func, maxfunc;
	uint16_t vendor;
	uint8_t hdrtype;

	maxfunc = 0;
	for (func = 0; func <= maxfunc; func++) {
		hdrtype = PCIB_READ_CONFIG(dev, bus, slot, func,
		    PCIR_HDRTYPE, 1);
		if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
			continue;
		if (func == 0 && (hdrtype & PCIM_MFDEV))
			maxfunc = PCI_FUNCMAX;

		vendor = PCIB_READ_CONFIG(dev, bus, slot, func,
		    PCIR_VENDOR, 2);
		if (vendor == 0xffff)
			continue;

		if ((hdrtype & PCIM_HDRTYPE) == PCIM_HDRTYPE_NORMAL)
			fdt_pci_config_normal(dev, bus, slot, func);
		else
			secbus = fdt_pci_config_bridge(dev, bus, secbus,
			    slot, func);
	}

	return (secbus);
}

static int
fdt_pci_config_bus(device_t dev, int bus, int maxslot)
{
	int func, maxfunc, secbus, slot;

	secbus = bus;
	for (slot = 0; slot <= maxslot; slot++)
		secbus = fdt_pci_config_slot(dev, bus, secbus, slot);

	return (secbus);
}

int
fdt_pci_config_domain(device_t dev)
{
	pcell_t bus_range[2];
	phandle_t root;
	int bus, error, maxslot;

	root = ofw_bus_get_node(dev);
	if (root == 0)
		return (EINVAL);
	if (!fdt_is_type(root, "pci"))
		return (EINVAL);

	/*
	 * Determine the bus number of the root in this domain.
	 * Lacking any information, this will be bus 0.
	 * Write the bus number to the bus device, using the IVAR.
	 */
	if ((OF_getprop(root, "bus-range", bus_range, sizeof(bus_range)) <= 0)
		bus = 0;
	else
		bus = fdt32_to_cpu(bus_range[0]);

	error = BUS_WRITE_IVAR(dev, NULL, PCIB_IVAR_BUS, bus);
	if (error)
		return (error);

	/* Get the maximum slot number for bus-enumeration. */
	maxslot = PCIB_MAXSLOTS(dev);

	bus = fdt_pci_config_bus(dev, bus, maxslot);
	return (0);
}
#endif
