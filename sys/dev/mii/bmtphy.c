/*-
 * Copyright (c) 1998, 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
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
 *
 *	from: NetBSD: bmtphy.c,v 1.8 2002/07/03 06:25:50 simonb Exp
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/bmtphy.c,v 1.12.2.7 2010/11/26 20:59:43 marius Exp $");

/*
 * Driver for the Broadcom BCM5201/BCM5202 "Mini-Theta" PHYs.  This also
 * drives the PHY on the 3Com 3c905C.  The 3c905C's PHY is described in
 * the 3c905C data sheet.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/bmtphyreg.h>

#include "miibus_if.h"

static int	bmtphy_probe(device_t);
static int	bmtphy_attach(device_t);

struct bmtphy_softc {
	struct mii_softc mii_sc;
	int mii_model;
};

static device_method_t bmtphy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bmtphy_probe),
	DEVMETHOD(device_attach,	bmtphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	{ 0, 0 }
};

static devclass_t	bmtphy_devclass;

static driver_t	bmtphy_driver = {
	"bmtphy",
	bmtphy_methods,
	sizeof(struct bmtphy_softc)
};

DRIVER_MODULE(bmtphy, miibus, bmtphy_driver, bmtphy_devclass, 0, 0);

static int	bmtphy_service(struct mii_softc *, struct mii_data *, int);
static void	bmtphy_status(struct mii_softc *);
static void	bmtphy_reset(struct mii_softc *);

static const struct mii_phydesc bmtphys_dp[] = {
	MII_PHY_DESC(BROADCOM, BCM4401),
	MII_PHY_DESC(BROADCOM, BCM5201),
	MII_PHY_DESC(BROADCOM, BCM5214),
	MII_PHY_DESC(BROADCOM, BCM5221),
	MII_PHY_DESC(BROADCOM, BCM5222),
	MII_PHY_END
};

static const struct mii_phydesc bmtphys_lp[] = {
	MII_PHY_DESC(BROADCOM, 3C905B),
	MII_PHY_DESC(BROADCOM, 3C905C),
	MII_PHY_END
};

static int
bmtphy_probe(device_t dev)
{
	int	rval;

	/* Let exphy(4) take precedence for these. */
	rval = mii_phy_dev_probe(dev, bmtphys_lp, BUS_PROBE_LOW_PRIORITY);
	if (rval <= 0)
		return (rval);

	return (mii_phy_dev_probe(dev, bmtphys_dp, BUS_PROBE_DEFAULT));
}

static int
bmtphy_attach(device_t dev)
{
	struct bmtphy_softc *bsc;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	bsc = device_get_softc(dev);
	sc = &bsc->mii_sc;
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = ma->mii_data;
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_flags = miibus_get_flags(dev);
	sc->mii_inst = mii->mii_instance++;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = bmtphy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOMANPAUSE;

	bsc->mii_model = MII_MODEL(ma->mii_id2);

	bmtphy_reset(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);

	return (0);
}

static int
bmtphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{

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
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	bmtphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
bmtphy_status(struct mii_softc *sc)
{
	struct mii_data *mii;
	struct ifmedia_entry *ife;
	int bmsr, bmcr, aux_csr;

	mii = sc->mii_pdata;
	ife = mii->mii_media.ifm_cur;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);

	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		/*
		 * The media status bits are only valid if autonegotiation
		 * has completed (or it's disabled).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		aux_csr = PHY_READ(sc, MII_BMTPHY_AUX_CSR);
		if (aux_csr & AUX_CSR_SPEED)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (aux_csr & AUX_CSR_FDX)
			mii->mii_media_active |=
			    IFM_FDX | mii_phy_flowstatus(sc);
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
bmtphy_reset(struct mii_softc *sc)
{
	struct bmtphy_softc *bsc;
	u_int16_t data;

	bsc = (struct bmtphy_softc *)sc;

	mii_phy_reset(sc);

	if (bsc->mii_model == MII_MODEL_BROADCOM_BCM5221) {
		/* Enable shadow register mode. */
		data = PHY_READ(sc, 0x1f);
		PHY_WRITE(sc, 0x1f, data | 0x0080);

		/* Enable APD (Auto PowerDetect). */
		data = PHY_READ(sc, MII_BMTPHY_AUX2);
		PHY_WRITE(sc, MII_BMTPHY_AUX2, data | 0x0020);

		/* Enable clocks across APD for Auto-MDIX functionality. */
		data = PHY_READ(sc, MII_BMTPHY_INTR);
		PHY_WRITE(sc, MII_BMTPHY_INTR, data | 0x0004);

		/* Disable shadow register mode. */
		data = PHY_READ(sc, 0x1f);
		PHY_WRITE(sc, 0x1f, data & ~0x0080);
	}
}
