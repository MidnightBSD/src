/*	$NetBSD: ukphy.c,v 1.2 1999/04/23 04:24:32 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Frank van der Linden.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/ukphy.c,v 1.20 2007/01/20 00:52:29 marius Exp $");

/*
 * driver for generic unknown PHYs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"

static int ukphy_probe(device_t);
static int ukphy_attach(device_t);

static device_method_t ukphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		ukphy_probe),
	DEVMETHOD(device_attach,	ukphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t ukphy_devclass;

static driver_t ukphy_driver = {
	"ukphy",
	ukphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(ukphy, miibus, ukphy_driver, ukphy_devclass, 0, 0);

static int	ukphy_service(struct mii_softc *, struct mii_data *, int);

static int
ukphy_probe(device_t dev)
{

	/*
	 * We know something is here, so always match at a low priority.
	 */
	device_set_desc(dev, "Generic IEEE 802.3u media interface");
	return (BUS_PROBE_GENERIC);
}

static int
ukphy_attach(device_t dev)
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	if (bootverbose)
		device_printf(dev, "OUI 0x%06x, model 0x%04x, rev. %d\n",
		    MII_OUI(ma->mii_id1, ma->mii_id2),
		    MII_MODEL(ma->mii_id2), MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = ukphy_service;
	sc->mii_pdata = mii;

	mii->mii_instance++;

	mii_phy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);
	mii_phy_setmedia(sc);

	return (0);
}

static int
ukphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	ukphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}
