/*-
 * Copyright (c) 2003 Jake Burkholder.
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
__FBSDID("$FreeBSD: src/sys/sparc64/fhc/fhc_central.c,v 1.8 2005/05/19 15:09:56 marius Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/fhc/fhcreg.h>
#include <sparc64/fhc/fhcvar.h>

static device_probe_t fhc_central_probe;
static device_attach_t fhc_central_attach;

static device_method_t fhc_central_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		fhc_central_probe),
	DEVMETHOD(device_attach,	fhc_central_attach),

	/* Bus interface. */
	DEVMETHOD(bus_print_child,	fhc_print_child),
	DEVMETHOD(bus_probe_nomatch,	fhc_probe_nomatch),
	DEVMETHOD(bus_setup_intr,	fhc_setup_intr),
	DEVMETHOD(bus_teardown_intr,	fhc_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	fhc_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_get_resource_list, fhc_get_resource_list),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_compat,	fhc_get_compat),
	DEVMETHOD(ofw_bus_get_model,	fhc_get_model),
	DEVMETHOD(ofw_bus_get_name,	fhc_get_name),
	DEVMETHOD(ofw_bus_get_node,	fhc_get_node),
	DEVMETHOD(ofw_bus_get_type,	fhc_get_type),

	{ NULL, NULL }
};

static driver_t fhc_central_driver = {
	"fhc",
	fhc_central_methods,
	sizeof(struct fhc_softc),
};

static devclass_t fhc_central_devclass;

DRIVER_MODULE(fhc, central, fhc_central_driver, fhc_central_devclass, 0, 0);

static int
fhc_central_probe(device_t dev)
{

	if (strcmp(ofw_bus_get_name(dev), "fhc") == 0) {
		device_set_desc(dev, "fhc");
		return (fhc_probe(dev));
	}
	return (ENXIO);
}

static int
fhc_central_attach(device_t dev)
{
	struct fhc_softc *sc;
	phandle_t node;
	int board;
	int rid;
	int i;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->sc_node = node;
	sc->sc_flags |= FHC_CENTRAL;

	for (i = 0; i < FHC_NREG; i++) {
		rid = i;
		sc->sc_memres[i] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &rid, RF_ACTIVE);
		if (sc->sc_memres[i] == NULL)
			panic("%s: can't allocate registers", __func__);
		sc->sc_bt[i] = rman_get_bustag(sc->sc_memres[i]);
		sc->sc_bh[i] = rman_get_bushandle(sc->sc_memres[i]);
	}

	board = bus_space_read_4(sc->sc_bt[FHC_INTERNAL],
	    sc->sc_bh[FHC_INTERNAL], FHC_BSR);
	sc->sc_board = ((board >> 16) & 0x1) | ((board >> 12) & 0xe);

	return (fhc_attach(dev));
}
