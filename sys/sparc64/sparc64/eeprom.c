/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Kranenburg.
 *	This product includes software developed by Harvard University.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)clock.c	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: clock.c,v 1.41 2001/07/24 19:29:25 eeh Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sparc64/sparc64/eeprom.c,v 1.9 2005/05/19 21:16:50 marius Exp $");

/*
 * clock (eeprom) attaches at EBus, FireHose or SBus
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/idprom.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <dev/mk48txx/mk48txxvar.h>

#include "clock_if.h"

#define	IDPROM_OFFSET	40

static devclass_t eeprom_devclass;

static device_probe_t eeprom_probe;
static device_attach_t eeprom_attach;

static device_method_t eeprom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eeprom_probe),
	DEVMETHOD(device_attach,	eeprom_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	mk48txx_gettime),
	DEVMETHOD(clock_settime,	mk48txx_settime),

	{ 0, 0 }
};

static driver_t eeprom_driver = {
	"eeprom",
	eeprom_methods,
	sizeof(struct mk48txx_softc),
};

DRIVER_MODULE(eeprom, ebus, eeprom_driver, eeprom_devclass, 0, 0);
DRIVER_MODULE(eeprom, fhc, eeprom_driver, eeprom_devclass, 0, 0);
DRIVER_MODULE(eeprom, sbus, eeprom_driver, eeprom_devclass, 0, 0);

static int
eeprom_probe(device_t dev)
{
 
	if (strcmp("eeprom", ofw_bus_get_name(dev)) == 0) {
		device_set_desc(dev, "EEPROM/clock");
		return (0);
	}
	return (ENXIO);
}

static int
eeprom_attach(device_t dev)
{
	struct mk48txx_softc *sc;
	struct resource *res;
	struct timespec ts;
	uint32_t h;
	int error, i, rid;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(struct mk48txx_softc));

	mtx_init(&sc->sc_mtx, "eeprom_mtx", NULL, MTX_DEF);

	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "cannot allocate resources\n");
		error = ENXIO;
		goto fail_mtx;
	}
	sc->sc_bst = rman_get_bustag(res);
	sc->sc_bsh = rman_get_bushandle(res);

	if ((sc->sc_model = ofw_bus_get_model(dev)) == NULL) {
		device_printf(dev, "cannot determine model\n");
		error = ENXIO;
		goto fail_res;
	}

	/* Our TOD clock year 0 is 1968 */
	sc->sc_year0 = 1968;
	/* Use default register read/write functions. */
	sc->sc_flag = 0;
	if ((error = mk48txx_attach(dev)) != 0) {
		device_printf(dev, "cannot attach time of day clock\n");
		goto fail_res;
	}

	/*
	 * Get the hostid from the NVRAM. This serves no real purpose other
	 * than being able to display it below as not all sparc64 models
	 * have an `eeprom' device and even some that do store the hostid
	 * elsewhere. The hostid in the NVRAM of the MK48Txx reads all zero
	 * on the latter models. A generic way to retrieve the hostid is to
	 * use the `idprom' node.
	 */
	mtx_lock(&sc->sc_mtx);
	h = bus_space_read_1(sc->sc_bst, sc->sc_bsh, sc->sc_nvramsz -
	    IDPROM_OFFSET + offsetof(struct idprom, id_machine)) << 24;
	for (i = 0; i < 3; i++) {
		h |= bus_space_read_1(sc->sc_bst, sc->sc_bsh, sc->sc_nvramsz -
		    IDPROM_OFFSET + offsetof(struct idprom, id_hostid[i])) <<
		    ((2 - i) * 8);
	}
	mtx_unlock(&sc->sc_mtx);
	if (h != 0)
		device_printf(dev, "hostid %x\n", (u_int)h);

	if (bootverbose) {
		mk48txx_gettime(dev, &ts);
		device_printf(dev, "current time: %ld.%09ld\n", (long)ts.tv_sec,
		    ts.tv_nsec);
	}

	return (0);

 fail_res:
	bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
 fail_mtx:
	mtx_destroy(&sc->sc_mtx);

	return (error);
}
