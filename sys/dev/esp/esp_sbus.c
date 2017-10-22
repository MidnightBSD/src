/*-
 * Copyright (c) 2004 Scott Long
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
 *
 */

/*	$NetBSD: esp_sbus.c,v 1.31 2005/02/27 00:27:48 perry Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center; Paul Kranenburg.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/esp/esp_sbus.c 166901 2007-02-23 12:19:07Z piso $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>

#include <sparc64/sbus/lsi64854reg.h>
#include <sparc64/sbus/lsi64854var.h>
#include <sparc64/sbus/sbusvar.h>

#include <dev/esp/ncr53c9xreg.h>
#include <dev/esp/ncr53c9xvar.h>

/* #define ESP_SBUS_DEBUG */

struct esp_softc {
	struct ncr53c9x_softc	sc_ncr53c9x;	/* glue to MI code */
	struct device		*sc_dev;

	int			sc_rid;
	struct resource		*sc_res;
	bus_space_handle_t	sc_regh;
	bus_space_tag_t		sc_regt;

	int			sc_irqrid;
	struct resource		*sc_irqres;
	void			*sc_irq;

	struct lsi64854_softc	*sc_dma;	/* pointer to my DMA */
};

static devclass_t	esp_devclass;

static int	esp_probe(device_t);
static int	esp_dma_attach(device_t);
static int	esp_dma_detach(device_t);
static int	esp_sbus_attach(device_t);
static int	esp_sbus_detach(device_t);
static int	esp_suspend(device_t);
static int	esp_resume(device_t);

static device_method_t esp_dma_methods[] = {
	DEVMETHOD(device_probe,		esp_probe),
	DEVMETHOD(device_attach,	esp_dma_attach),
	DEVMETHOD(device_detach,	esp_dma_detach),
	DEVMETHOD(device_suspend,	esp_suspend),
	DEVMETHOD(device_resume,	esp_resume),
	{0, 0}
};

static driver_t esp_dma_driver = {
	"esp",
	esp_dma_methods,
	sizeof(struct esp_softc)
};

DRIVER_MODULE(esp, dma, esp_dma_driver, esp_devclass, 0, 0);
MODULE_DEPEND(esp, dma, 1, 1, 1);

static device_method_t esp_sbus_methods[] = {
	DEVMETHOD(device_probe,		esp_probe),
	DEVMETHOD(device_attach,	esp_sbus_attach),
	DEVMETHOD(device_detach,	esp_sbus_detach),
	DEVMETHOD(device_suspend,	esp_suspend),
	DEVMETHOD(device_resume,	esp_resume),
	{0, 0}
};

static driver_t esp_sbus_driver = {
	"esp",
	esp_sbus_methods,
	sizeof(struct esp_softc)
};

DRIVER_MODULE(esp, sbus, esp_sbus_driver, esp_devclass, 0, 0);
MODULE_DEPEND(esp, sbus, 1, 1, 1);

MODULE_DEPEND(esp, cam, 1, 1, 1);

/*
 * Functions and the switch for the MI code.
 */
static u_char	esp_read_reg(struct ncr53c9x_softc *, int);
static void	esp_write_reg(struct ncr53c9x_softc *, int, u_char);
static int	esp_dma_isintr(struct ncr53c9x_softc *);
static void	esp_dma_reset(struct ncr53c9x_softc *);
static int	esp_dma_intr(struct ncr53c9x_softc *);
static int	esp_dma_setup(struct ncr53c9x_softc *, caddr_t *, size_t *,
			      int, size_t *);
static void	esp_dma_go(struct ncr53c9x_softc *);
static void	esp_dma_stop(struct ncr53c9x_softc *);
static int	esp_dma_isactive(struct ncr53c9x_softc *);
static int	espattach(struct esp_softc *, struct ncr53c9x_glue *);

static struct ncr53c9x_glue esp_sbus_glue = {
	esp_read_reg,
	esp_write_reg,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

static int
esp_probe(device_t dev)
{
	const char *name;

	name = ofw_bus_get_name(dev);
	if (strcmp("SUNW,fas", name) == 0) {
		device_set_desc(dev, "Sun FAS366 Fast-Wide SCSI");
	        return (BUS_PROBE_DEFAULT);
	} else if (strcmp("esp", name) == 0) {
		device_set_desc(dev, "Sun ESP SCSI/Sun FAS Fast-SCSI");
	        return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
esp_sbus_attach(device_t dev)
{
	struct esp_softc *esc;
	struct ncr53c9x_softc *sc;
	struct lsi64854_softc *lsc;
	device_t *children;
	const char *name;
	phandle_t node;
	int burst, error, i, nchildren, slot;

	esc = device_get_softc(dev);
	bzero(esc, sizeof(struct esp_softc));
	sc = &esc->sc_ncr53c9x;

	lsc = NULL;
	esc->sc_dev = dev;
	name = ofw_bus_get_name(dev);
	node = ofw_bus_get_node(dev);
	if (OF_getprop(node, "initiator-id", &sc->sc_id,
	    sizeof(sc->sc_id)) == -1)
		sc->sc_id = 7;
	sc->sc_freq = sbus_get_clockfreq(dev);

#ifdef ESP_SBUS_DEBUG
	device_printf(dev, "%s: sc_id %d, freq %d\n", __func__, sc->sc_id,
	    sc->sc_freq);
#endif

	if (strcmp(name, "SUNW,fas") == 0) {
		/*
		 * Allocate space for DMA, in SUNW,fas there are no
		 * separate DMA devices.
		 */
		lsc = malloc(sizeof (struct lsi64854_softc), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (lsc == NULL) {
			device_printf(dev, "out of memory (lsi64854_softc)\n");
			return (ENOMEM);
		}
		esc->sc_dma = lsc;

		/*
		 * SUNW,fas have 2 register spaces: DMA (lsi64854) and
		 * SCSI core (ncr53c9x).
		 */

		/* Allocate DMA registers. */
		lsc->sc_rid = 0;
		if ((lsc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &lsc->sc_rid, RF_ACTIVE)) == NULL) {
			device_printf(dev, "cannot allocate DMA registers\n");
			error = ENXIO;
			goto fail_sbus_lsc;
		}
		lsc->sc_regt = rman_get_bustag(lsc->sc_res);
		lsc->sc_regh = rman_get_bushandle(lsc->sc_res);

		/* Create a parent DMA tag based on this bus. */
		error = bus_dma_tag_create(
		    bus_get_dma_tag(dev),	/* parent */
		    PAGE_SIZE, 0,		/* alignment, boundary */
		    BUS_SPACE_MAXADDR,		/* lowaddr */
		    BUS_SPACE_MAXADDR,		/* highaddr */
		    NULL, NULL,			/* filter, filterarg */
		    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
		    0,				/* nsegments */
		    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
		    0,				/* flags */
		    NULL, NULL,			/* no locking */
		    &lsc->sc_parent_dmat);
		if (error != 0) {
			device_printf(dev, "cannot allocate parent DMA tag\n");
			goto fail_sbus_lres;
		}
		burst = sbus_get_burstsz(dev);

#ifdef ESP_SBUS_DEBUG
		printf("%s: burst 0x%x\n", __func__, burst);
#endif

		lsc->sc_burst = (burst & SBUS_BURST_32) ? 32 :
		    (burst & SBUS_BURST_16) ? 16 : 0;

		lsc->sc_channel = L64854_CHANNEL_SCSI;
		lsc->sc_client = sc;
		lsc->sc_dev = dev;

		error = lsi64854_attach(lsc);
		if (error != 0) {
			device_printf(dev, "lsi64854_attach failed\n");
			goto fail_sbus_lpdma;
		}

		/*
		 * Allocate SCSI core registers.
		 */
		esc->sc_rid = 1;
		if ((esc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &esc->sc_rid, RF_ACTIVE)) == NULL) {
			device_printf(dev,
			    "cannot allocate SCSI core registers\n");
			error = ENXIO;
			goto fail_sbus_lsi;
		}
		esc->sc_regt = rman_get_bustag(esc->sc_res);
		esc->sc_regh = rman_get_bushandle(esc->sc_res);
	} else {
		/*
		 * Search accompanying DMA engine. It should have been
		 * already attached otherwise there isn't much we can do.
		 */
		if (device_get_children(device_get_parent(dev), &children,
		    &nchildren) != 0) {
			device_printf(dev, "cannot determine siblings\n");
			return (ENXIO);
		}
		slot = sbus_get_slot(dev);
		for (i = 0; i < nchildren; i++) {
			if (device_is_attached(children[i]) &&
			    sbus_get_slot(children[i]) == slot &&
			    strcmp(ofw_bus_get_name(children[i]), "dma") == 0) {
				/* XXX hackery */
				esc->sc_dma = (struct lsi64854_softc *)
				    device_get_softc(children[i]);
				break;
			}
		}
		free(children, M_TEMP);
		if (esc->sc_dma == NULL) {
			device_printf(dev, "cannot find DMA engine\n");
			return (ENXIO);
		}
		esc->sc_dma->sc_client = sc;

		/*
		 * Allocate SCSI core registers.
		 */
		esc->sc_rid = 0;
		if ((esc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &esc->sc_rid, RF_ACTIVE)) == NULL) {
			device_printf(dev,
			    "cannot allocate SCSI core registers\n");
			return (ENXIO);
		}
		esc->sc_regt = rman_get_bustag(esc->sc_res);
		esc->sc_regh = rman_get_bushandle(esc->sc_res);
	}

	error = espattach(esc, &esp_sbus_glue);
	if (error != 0) {
		device_printf(dev, "espattach failed\n");
		goto fail_sbus_eres;
	}

	return (0);

 fail_sbus_eres:
	bus_release_resource(dev, SYS_RES_MEMORY, esc->sc_rid, esc->sc_res);
	if (strcmp(name, "SUNW,fas") != 0)
		return (error);
 fail_sbus_lsi:
	lsi64854_detach(lsc);
 fail_sbus_lpdma:
	bus_dma_tag_destroy(lsc->sc_parent_dmat);
 fail_sbus_lres:
	bus_release_resource(dev, SYS_RES_MEMORY, lsc->sc_rid, lsc->sc_res);
 fail_sbus_lsc:
	free(lsc, M_DEVBUF);
	return (error);
}

static int
esp_sbus_detach(device_t dev)
{
	struct esp_softc *esc;
	struct ncr53c9x_softc *sc;
	struct lsi64854_softc *lsc;
	int error;

	esc = device_get_softc(dev);
	sc = &esc->sc_ncr53c9x;
	lsc = esc->sc_dma;

	bus_teardown_intr(esc->sc_dev, esc->sc_irqres, esc->sc_irq);
	error = ncr53c9x_detach(sc);
	if (error != 0)
		return (error);
	bus_release_resource(esc->sc_dev, SYS_RES_IRQ, esc->sc_irqrid,
	    esc->sc_irqres);
	bus_release_resource(dev, SYS_RES_MEMORY, esc->sc_rid, esc->sc_res);
	if (strcmp(ofw_bus_get_name(dev), "SUNW,fas") != 0)
		return (0);
	error = lsi64854_detach(lsc);
	if (error != 0)
		return (error);
	bus_dma_tag_destroy(lsc->sc_parent_dmat);
	bus_release_resource(dev, SYS_RES_MEMORY, lsc->sc_rid, lsc->sc_res);
	free(lsc, M_DEVBUF);

	return (0);
}

static int
esp_dma_attach(device_t dev)
{
	struct esp_softc *esc;
	struct ncr53c9x_softc *sc;
	phandle_t node;
	int error;

	esc = device_get_softc(dev);
	bzero(esc, sizeof(struct esp_softc));
	sc = &esc->sc_ncr53c9x;

	esc->sc_dev = dev;
	node = ofw_bus_get_node(dev);
	if (OF_getprop(node, "initiator-id", &sc->sc_id,
	    sizeof(sc->sc_id)) == -1)
		sc->sc_id = 7;
	if (OF_getprop(node, "clock-frequency", &sc->sc_freq,
	    sizeof(sc->sc_freq)) == -1) {
		printf("failed to query OFW for clock-frequency\n");
		return (ENXIO);
	}

#ifdef ESP_SBUS_DEBUG
	device_printf(dev, "%s: sc_id %d, freq %d\n", __func__, sc->sc_id,
	    sc->sc_freq);
#endif

	/* XXX hackery */
	esc->sc_dma = (struct lsi64854_softc *)
	    device_get_softc(device_get_parent(dev));
	esc->sc_dma->sc_client = sc;

	/*
	 * Allocate SCSI core registers.
	 */
	esc->sc_rid = 0;
	if ((esc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &esc->sc_rid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "cannot allocate SCSI core registers\n");
		return (ENXIO);
	}
	esc->sc_regt = rman_get_bustag(esc->sc_res);
	esc->sc_regh = rman_get_bushandle(esc->sc_res);

	error = espattach(esc, &esp_sbus_glue);
	if (error != 0) {
		device_printf(dev, "espattach failed\n");
		goto fail_dma_eres;
	}

	return (0);

 fail_dma_eres:
	bus_release_resource(dev, SYS_RES_MEMORY, esc->sc_rid, esc->sc_res);
	return (error);
}

static int
esp_dma_detach(device_t dev)
{
	struct esp_softc *esc;
	struct ncr53c9x_softc *sc;
	int error;

	esc = device_get_softc(dev);
	sc = &esc->sc_ncr53c9x;

	bus_teardown_intr(esc->sc_dev, esc->sc_irqres, esc->sc_irq);
	error = ncr53c9x_detach(sc);
	if (error != 0)
		return (error);
	bus_release_resource(esc->sc_dev, SYS_RES_IRQ, esc->sc_irqrid,
	    esc->sc_irqres);
	bus_release_resource(dev, SYS_RES_MEMORY, esc->sc_rid, esc->sc_res);

	return (0);
}

static int
esp_suspend(device_t dev)
{

	return (ENXIO);
}

static int
esp_resume(device_t dev)
{

	return (ENXIO);
}

/*
 * Attach this instance, and then all the sub-devices
 */
static int
espattach(struct esp_softc *esc, struct ncr53c9x_glue *gluep)
{
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	unsigned int uid = 0;
	int error;

	/*
	 * The `ESC' DMA chip must be reset before we can access
	 * the ESP registers.
	 */
	if (esc->sc_dma->sc_rev == DMAREV_ESC)
		DMA_RESET(esc->sc_dma);

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = gluep;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * Read the part-unique ID code of the SCSI chip. The contained
	 * value is only valid if all of the following conditions are met:
	 * - After power-up or chip reset.
	 * - Before any value is written to this register.
	 * - The NCRCFG2_FE bit is set.
	 * - A (NCRCMD_NOP | NCRCMD_DMA) command has been issued.
	 */
	NCRCMD(sc, NCRCMD_RSTCHIP);
	NCRCMD(sc, NCRCMD_NOP);
	sc->sc_cfg2 = NCRCFG2_FE;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
	NCRCMD(sc, NCRCMD_NOP | NCRCMD_DMA);
	uid = NCR_READ_REG(sc, NCR_UID);

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the ncr53c9x_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	NCR_WRITE_REG(sc, NCR_CFG1, sc->sc_cfg1);
	sc->sc_cfg2 = 0;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_RPE;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);

	if ((NCR_READ_REG(sc, NCR_CFG2) & ~NCRCFG2_RSVD) !=
	    (NCRCFG2_SCSI2 | NCRCFG2_RPE)) {
		sc->sc_rev = NCR_VARIANT_ESP100;
	} else {
		sc->sc_cfg2 = NCRCFG2_SCSI2;
		NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (NCRCFG3_CDB | NCRCFG3_FCLK);
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		if (NCR_READ_REG(sc, NCR_CFG3) !=
		    (NCRCFG3_CDB | NCRCFG3_FCLK)) {
			sc->sc_rev = NCR_VARIANT_ESP100A;
		} else {
			/* NCRCFG2_FE enables > 64K transfers */
			sc->sc_cfg2 |= NCRCFG2_FE;
			sc->sc_cfg3 = 0;
			NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
			if (sc->sc_freq <= 25)
				sc->sc_rev = NCR_VARIANT_ESP200;
			else {
				switch ((uid & 0xf8) >> 3) {
				case 0x00:
					sc->sc_rev = NCR_VARIANT_FAS100A;
					break;
				case 0x02:
					if ((uid & 0x07) == 0x02)
						sc->sc_rev = NCR_VARIANT_FAS216;
					else
						sc->sc_rev = NCR_VARIANT_FAS236;
					break;
				case 0x0a:
					sc->sc_rev = NCR_VARIANT_FAS366;
					break;
				default:
					/*
					 * We could just treat unknown chips
					 * as ESP200 but then we would most
					 * likely drive them out of specs.
					 */
					device_printf(esc->sc_dev,
					    "Unknown chip\n");
					return (ENXIO);
				}
			}
		}
	}

#ifdef ESP_SBUS_DEBUG
	printf("%s: revision %d, uid 0x%x\n", __func__, sc->sc_rev, uid);
#endif

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	sc->sc_maxoffset = 15;
	sc->sc_extended_geom = 1;

	/*
	 * Alas, we must now modify the value a bit, because it's
	 * only valid when can switch on FASTCLK and FASTSCSI bits
	 * in config register 3...
	 */
	switch (sc->sc_rev) {
	case NCR_VARIANT_ESP100:
		sc->sc_maxwidth = 0;
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;

	case NCR_VARIANT_ESP100A:
		sc->sc_maxwidth = 0;
		sc->sc_maxxfer = 64 * 1024;
		/* Min clocks/byte is 5 */
		sc->sc_minsync = ncr53c9x_cpb2stp(sc, 5);
		break;

	case NCR_VARIANT_ESP200:
		sc->sc_maxwidth = 0;
		sc->sc_maxxfer = 16 * 1024 * 1024;
		/* Min clocks/byte is 5 */
		sc->sc_minsync = ncr53c9x_cpb2stp(sc, 5);
		break;

	case NCR_VARIANT_FAS100A:
	case NCR_VARIANT_FAS216:
	case NCR_VARIANT_FAS236:
		/*
		 * The onboard SCSI chips in Sun Ultra 1 are actually
		 * documented to be NCR53C9X which use NCRCFG3_FCLK and
		 * NCRCFG3_FSCSI. BSD/OS however probes these chips as
		 * FAS100A and uses NCRF9XCFG3_FCLK and NCRF9XCFG3_FSCSI
		 * instead which seems to be correct as otherwise sync
		 * negotiation just doesn't work. Using NCRF9XCFG3_FCLK
		 * and NCRF9XCFG3_FSCSI with these chips in fact also
		 * yields Fast-SCSI speed.
		 */
		sc->sc_features = NCR_F_FASTSCSI;
		sc->sc_cfg3 = NCRF9XCFG3_FCLK;
		sc->sc_cfg3_fscsi = NCRF9XCFG3_FSCSI;
		sc->sc_maxwidth = 0;
		sc->sc_maxxfer = 16 * 1024 * 1024;
		break;

	case NCR_VARIANT_FAS366:
		sc->sc_maxwidth = 1;
		sc->sc_maxxfer = 16 * 1024 * 1024;
		break;
	}

	/* Limit minsync due to unsolved performance issues. */
	sc->sc_maxsync = sc->sc_minsync;

	/* Establish interrupt channel */
	esc->sc_irqrid = 0;
	if ((esc->sc_irqres = bus_alloc_resource_any(esc->sc_dev, SYS_RES_IRQ,
	    &esc->sc_irqrid, RF_SHAREABLE|RF_ACTIVE)) == NULL) {
		device_printf(esc->sc_dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}
	if (bus_setup_intr(esc->sc_dev, esc->sc_irqres,
	    INTR_TYPE_BIO|INTR_MPSAFE, NULL, ncr53c9x_intr, sc, &esc->sc_irq)) {
		device_printf(esc->sc_dev, "cannot set up interrupt\n");
		error = ENXIO;
		goto fail_ires;
	}

	/* Turn on target selection using the `DMA' method */
	if (sc->sc_rev != NCR_VARIANT_FAS366)
		sc->sc_features |= NCR_F_DMASELECT;

	/* Do the common parts of attachment. */
	sc->sc_dev = esc->sc_dev;
	error = ncr53c9x_attach(sc);
	if (error != 0) {
		device_printf(esc->sc_dev, "ncr53c9x_attach failed\n");
		goto fail_intr;
	}

	return (0);

 fail_intr:
	bus_teardown_intr(esc->sc_dev, esc->sc_irqres, esc->sc_irq);
 fail_ires:
	bus_release_resource(esc->sc_dev, SYS_RES_IRQ, esc->sc_irqrid,
	    esc->sc_irqres);
	return (error);
}

/*
 * Glue functions.
 */

#ifdef ESP_SBUS_DEBUG
int esp_sbus_debug = 0;

static struct {
	char *r_name;
	int   r_flag;
} esp__read_regnames [] = {
	{ "TCL", 0},			/* 0/00 */
	{ "TCM", 0},			/* 1/04 */
	{ "FIFO", 0},			/* 2/08 */
	{ "CMD", 0},			/* 3/0c */
	{ "STAT", 0},			/* 4/10 */
	{ "INTR", 0},			/* 5/14 */
	{ "STEP", 0},			/* 6/18 */
	{ "FFLAGS", 1},			/* 7/1c */
	{ "CFG1", 1},			/* 8/20 */
	{ "STAT2", 0},			/* 9/24 */
	{ "CFG4", 1},			/* a/28 */
	{ "CFG2", 1},			/* b/2c */
	{ "CFG3", 1},			/* c/30 */
	{ "-none", 1},			/* d/34 */
	{ "TCH", 1},			/* e/38 */
	{ "TCX", 1},			/* f/3c */
};

static struct {
	char *r_name;
	int   r_flag;
} esp__write_regnames[] = {
	{ "TCL", 1},			/* 0/00 */
	{ "TCM", 1},			/* 1/04 */
	{ "FIFO", 0},			/* 2/08 */
	{ "CMD", 0},			/* 3/0c */
	{ "SELID", 1},			/* 4/10 */
	{ "TIMEOUT", 1},		/* 5/14 */
	{ "SYNCTP", 1},			/* 6/18 */
	{ "SYNCOFF", 1},		/* 7/1c */
	{ "CFG1", 1},			/* 8/20 */
	{ "CCF", 1},			/* 9/24 */
	{ "TEST", 1},			/* a/28 */
	{ "CFG2", 1},			/* b/2c */
	{ "CFG3", 1},			/* c/30 */
	{ "-none", 1},			/* d/34 */
	{ "TCH", 1},			/* e/38 */
	{ "TCX", 1},			/* f/3c */
};
#endif

static u_char
esp_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v;

	v = bus_space_read_1(esc->sc_regt, esc->sc_regh, reg * 4);
#ifdef ESP_SBUS_DEBUG
	if (esp_sbus_debug && (reg < 0x10) && esp__read_regnames[reg].r_flag)
		printf("RD:%x <%s> %x\n", reg * 4,
		    ((unsigned)reg < 0x10) ? esp__read_regnames[reg].r_name : "<***>", v);
#endif
	return v;
}

static void
esp_write_reg(struct ncr53c9x_softc *sc, int reg, u_char v)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

#ifdef ESP_SBUS_DEBUG
	if (esp_sbus_debug && (reg < 0x10) && esp__write_regnames[reg].r_flag)
		printf("WR:%x <%s> %x\n", reg * 4,
		    ((unsigned)reg < 0x10) ? esp__write_regnames[reg].r_name : "<***>", v);
#endif
	bus_space_write_1(esc->sc_regt, esc->sc_regh, reg * 4, v);
}

static int
esp_dma_isintr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_ISINTR(esc->sc_dma));
}

static void
esp_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_RESET(esc->sc_dma);
}

static int
esp_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_INTR(esc->sc_dma));
}

static int
esp_dma_setup(struct ncr53c9x_softc *sc, caddr_t *addr, size_t *len,
	      int datain, size_t *dmasize)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_SETUP(esc->sc_dma, addr, len, datain, dmasize));
}

static void
esp_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_GO(esc->sc_dma);
}

static void
esp_dma_stop(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	uint32_t csr;

	csr = L64854_GCSR(esc->sc_dma);
	csr &= ~D_EN_DMA;
	L64854_SCSR(esc->sc_dma, csr);
}

static int
esp_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_ISACTIVE(esc->sc_dma));
}
