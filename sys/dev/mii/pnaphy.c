/*-
 * Copyright (c) 2000 Berkeley Software Design, Inc.
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@osd.bsdi.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/pnaphy.c,v 1.18 2006/12/02 15:32:33 marius Exp $");

/*
 * driver for homePNA PHYs
 * This is really just a stub that allows us to identify homePNA-based
 * transceicers and display the link status. MII-based homePNA PHYs
 * only support one media type and no autonegotiation. If we were
 * really clever, we could tweak some of the vendor-specific registers
 * to optimize the link.
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
#include "miidevs.h"

#include "miibus_if.h"

static int pnaphy_probe(device_t);
static int pnaphy_attach(device_t);

static device_method_t pnaphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		pnaphy_probe),
	DEVMETHOD(device_attach,	pnaphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t pnaphy_devclass;

static driver_t pnaphy_driver = {
	"pnaphy",
	pnaphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(pnaphy, miibus, pnaphy_driver, pnaphy_devclass, 0, 0);

static int	pnaphy_service(struct mii_softc *, struct mii_data *,int);

static const struct mii_phydesc pnaphys[] = {
	MII_PHY_DESC(AMD, 79c978),
	MII_PHY_END
};

static int
pnaphy_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, pnaphys, BUS_PROBE_DEFAULT));
}

static int
pnaphy_attach(device_t dev)
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *sep = "";

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = pnaphy_service;
	sc->mii_pdata = mii;

	mii->mii_instance++;

	sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define PRINT(s)	printf("%s%s", sep, s); sep = ", "

	mii_phy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_HPNA_1, 0, sc->mii_inst), 0);
		PRINT("HomePNA");
	}
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);

	printf("\n");

#undef ADD
#undef PRINT

	MIIBUS_MEDIAINIT(sc->mii_dev);

	return (0);
}

static int
pnaphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
		case IFM_10_T:
		case IFM_100_TX:
		case IFM_100_T4:
			return (EINVAL);
		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
		}
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
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_10_T)
		mii->mii_media_active = IFM_ETHER|IFM_HPNA_1;

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}
