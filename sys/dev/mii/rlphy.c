/*-
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/mii/rlphy.c 170367 2007-06-06 07:07:23Z yongari $");

/*
 * driver for RealTek 8139 internal PHYs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <machine/bus.h>
#include <pci/if_rlreg.h>

#include "miibus_if.h"

struct rlphy_softc {
	struct mii_softc sc_mii;	/* generic PHY */
	int sc_is_RTL8201L;		/* is an external RTL8201L PHY */
};

static int rlphy_probe(device_t);
static int rlphy_attach(device_t);

static device_method_t rlphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		rlphy_probe),
	DEVMETHOD(device_attach,	rlphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t rlphy_devclass;

static driver_t rlphy_driver = {
	"rlphy",
	rlphy_methods,
	sizeof(struct rlphy_softc)
};

DRIVER_MODULE(rlphy, miibus, rlphy_driver, rlphy_devclass, 0, 0);

static int	rlphy_service(struct mii_softc *, struct mii_data *, int);
static void	rlphy_status(struct mii_softc *);

/*
 * RealTek internal PHYs don't have vendor/device ID registers;
 * re(4) and rl(4) fake up a return value of all zeros.
 */
static const struct mii_phydesc rlintphys[] = {
	{ 0, 0, "RealTek internal media interface" },
	MII_PHY_END
};

static const struct mii_phydesc rlphys[] = {
	MII_PHY_DESC(REALTEK, RTL8201L),
	MII_PHY_DESC(ICPLUS, IP101),
	MII_PHY_END
};

static int
rlphy_probe(device_t dev)
{
	const char *nic;
	int rv;

	rv = mii_phy_dev_probe(dev, rlphys, BUS_PROBE_DEFAULT);
	if (rv <= 0)
		return (rv);

	nic = device_get_name(device_get_parent(device_get_parent(dev)));
	if (strcmp(nic, "rl") == 0 || strcmp(nic, "re") == 0)
		return (mii_phy_dev_probe(dev, rlintphys, BUS_PROBE_DEFAULT));
	return (ENXIO);
}

static int
rlphy_attach(device_t dev)
{
	struct mii_softc	*sc;
	struct mii_attach_args	*ma;
	struct mii_data		*mii;
	struct rlphy_softc 	*rsc;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);

        /*
         * Check whether we're the RTL8201L PHY and remember so the status
         * routine can query the proper register for speed detection.
         */
	rsc = (struct rlphy_softc *)sc;
	if (mii_phy_dev_probe(dev, rlphys, 0) == 0)
		rsc->sc_is_RTL8201L++;
	
	/*
	 * The RealTek PHY can never be isolated, so never allow non-zero
	 * instances!
	 */
	if (mii->mii_instance != 0) {
		device_printf(dev, "ignoring this PHY, non-zero instance\n");
		return (ENXIO);
	}

	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = rlphy_service;
	sc->mii_pdata = mii;
	mii->mii_instance++;

	sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    MII_MEDIA_100_TX);

	mii_phy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");
#undef ADD
	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static int
rlphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	/*
	 * We can't isolate the RealTek PHY, so it has to be the only one!
	 */
	if (IFM_INST(ife->ifm_media) != sc->mii_inst)
		panic("rlphy_service: can't isolate RealTek PHY");

	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * The RealTek PHY's autonegotiation doesn't need to be
		 * kicked; it continues in the background.
		 */
		break;
	}

	/* Update the media status. */
	rlphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
rlphy_status(struct mii_softc *phy)
{
	struct rlphy_softc *rsc =(struct rlphy_softc *)phy;
	struct mii_data *mii = phy->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(phy, MII_BMSR) | PHY_READ(phy, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(phy, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		/*
		 * NWay autonegotiation takes the highest-order common
		 * bit of the ANAR and ANLPAR (i.e. best media advertised
		 * both by us and our link partner).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if ((anlpar = PHY_READ(phy, MII_ANAR) &
		    PHY_READ(phy, MII_ANLPAR))) {
			if (anlpar & ANLPAR_T4)
				mii->mii_media_active |= IFM_100_T4;
			else if (anlpar & ANLPAR_TX_FD)
				mii->mii_media_active |= IFM_100_TX|IFM_FDX;
			else if (anlpar & ANLPAR_TX)
				mii->mii_media_active |= IFM_100_TX;
			else if (anlpar & ANLPAR_10_FD)
				mii->mii_media_active |= IFM_10_T|IFM_FDX;
			else if (anlpar & ANLPAR_10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_NONE;
			return;
		}
		/*
		 * If the other side doesn't support NWAY, then the
		 * best we can do is determine if we have a 10Mbps or
		 * 100Mbps link. There's no way to know if the link
		 * is full or half duplex, so we default to half duplex
		 * and hope that the user is clever enough to manually
		 * change the media settings if we're wrong.
		 */

		/*
		 * The RealTek PHY supports non-NWAY link speed
		 * detection, however it does not report the link
		 * detection results via the ANLPAR or BMSR registers.
		 * (What? RealTek doesn't do things the way everyone
		 * else does? I'm just shocked, shocked I tell you.)
		 * To determine the link speed, we have to do one
		 * of two things:
		 *
		 * - If this is a standalone RealTek RTL8201(L) PHY,
		 *   we can determine the link speed by testing bit 0
		 *   in the magic, vendor-specific register at offset
		 *   0x19.
		 *
		 * - If this is a RealTek MAC with integrated PHY, we
		 *   can test the 'SPEED10' bit of the MAC's media status
		 *   register.
		 */
		if (rsc->sc_is_RTL8201L) {
			if (PHY_READ(phy, 0x0019) & 0x01)
				mii->mii_media_active |= IFM_100_TX;
			else
				mii->mii_media_active |= IFM_10_T;
		} else {
			if (PHY_READ(phy, RL_MEDIASTAT) &
			    RL_MEDIASTAT_SPEED10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_100_TX;
		}
	} else
		mii->mii_media_active = ife->ifm_media;
}
