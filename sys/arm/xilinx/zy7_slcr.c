/*-
 * Copyright (c) 2013 Thomas Skibo
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
 * $FreeBSD: release/10.0.0/sys/arm/xilinx/zy7_slcr.c 250015 2013-04-28 07:00:36Z wkoszek $
 */

/*
 * Zynq-700 SLCR driver.  Provides hooks for cpu_reset and PL control stuff.
 * In the future, maybe MIO control, clock control, etc. could go here.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.4) November 16, 2012.  Xilinx doc UG585.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/arm/xilinx/zy7_slcr.c 250015 2013-04-28 07:00:36Z wkoszek $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/xilinx/zy7_slcr.h>

struct zy7_slcr_softc {
	device_t	dev;
	struct mtx	sc_mtx;
	struct resource	*mem_res;
};

static struct zy7_slcr_softc *zy7_slcr_softc_p;
extern void (*zynq7_cpu_reset);

#define ZSLCR_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	ZSLCR_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define ZSLCR_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	\
	    "zy7_slcr", MTX_SPIN)
#define ZSLCR_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

#define RD4(sc, off) 		(bus_read_4((sc)->mem_res, (off)))
#define WR4(sc, off, val) 	(bus_write_4((sc)->mem_res, (off), (val)))


SYSCTL_NODE(_hw, OID_AUTO, zynq, CTLFLAG_RD, 0, "Xilinx Zynq-7000");

static char zynq_bootmode[64];
SYSCTL_STRING(_hw_zynq, OID_AUTO, bootmode, CTLFLAG_RD, zynq_bootmode, 0,
	      "Zynq boot mode");

static char zynq_pssid[80];
SYSCTL_STRING(_hw_zynq, OID_AUTO, pssid, CTLFLAG_RD, zynq_pssid, 0,
	   "Zynq PSS IDCODE");

static uint32_t zynq_reboot_status;
SYSCTL_INT(_hw_zynq, OID_AUTO, reboot_status, CTLFLAG_RD, &zynq_reboot_status,
	   0, "Zynq REBOOT_STATUS register");

static void
zy7_slcr_unlock(struct zy7_slcr_softc *sc)
{

	/* Unlock SLCR with magic number. */
	WR4(sc, ZY7_SLCR_UNLOCK, ZY7_SLCR_UNLOCK_MAGIC);
}

static void
zy7_slcr_lock(struct zy7_slcr_softc *sc)
{

	/* Lock SLCR with magic number. */
	WR4(sc, ZY7_SLCR_LOCK, ZY7_SLCR_LOCK_MAGIC);
}


static void
zy7_slcr_cpu_reset(void)
{
	struct zy7_slcr_softc *sc = zy7_slcr_softc_p;

	/* Unlock SLCR registers. */
	zy7_slcr_unlock(sc);

	/* This has something to do with a work-around so the fsbl will load
	 * the bitstream after soft-reboot.  It's very important.
	 */
	WR4(sc, ZY7_SLCR_REBOOT_STAT,
	    RD4(sc, ZY7_SLCR_REBOOT_STAT) & 0xf0ffffff);

	/* Soft reset */
	WR4(sc, ZY7_SLCR_PSS_RST_CTRL, ZY7_SLCR_PSS_RST_CTRL_SOFT_RESET);

	for (;;)
		;
}

/* Assert PL resets and disable level shifters in preparation of programming
 * the PL (FPGA) section.  Called from zy7_devcfg.c.
 */
void
zy7_slcr_preload_pl(void)
{
	struct zy7_slcr_softc *sc = zy7_slcr_softc_p;

	if (!sc)
		return;

	ZSLCR_LOCK(sc);

	/* Unlock SLCR registers. */
	zy7_slcr_unlock(sc);

	/* Assert top level output resets. */
	WR4(sc, ZY7_SLCR_FPGA_RST_CTRL, ZY7_SLCR_FPGA_RST_CTRL_RST_ALL);

	/* Disable all level shifters. */
	WR4(sc, ZY7_SLCR_LVL_SHFTR_EN, 0);

	/* Lock SLCR registers. */
	zy7_slcr_lock(sc);

	ZSLCR_UNLOCK(sc);
}

/* After PL configuration, enable level shifters and deassert top-level
 * PL resets.  Called from zy7_devcfg.c.  Optionally, the level shifters
 * can be left disabled but that's rare of an FPGA application. That option
 * is controled by a sysctl in the devcfg driver.
 */
void
zy7_slcr_postload_pl(int en_level_shifters)
{
	struct zy7_slcr_softc *sc = zy7_slcr_softc_p;

	if (!sc)
		return;

	ZSLCR_LOCK(sc);

	/* Unlock SLCR registers. */
	zy7_slcr_unlock(sc);

	if (en_level_shifters)
		/* Enable level shifters. */
		WR4(sc, ZY7_SLCR_LVL_SHFTR_EN, ZY7_SLCR_LVL_SHFTR_EN_ALL);

	/* Deassert top level output resets. */
	WR4(sc, ZY7_SLCR_FPGA_RST_CTRL, 0);

	/* Lock SLCR registers. */
	zy7_slcr_lock(sc);

	ZSLCR_UNLOCK(sc);
}

static int
zy7_slcr_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "xlnx,zy7_slcr"))
		return (ENXIO);

	device_set_desc(dev, "Zynq-7000 slcr block");
	return (0);
}

static int
zy7_slcr_attach(device_t dev)
{
	struct zy7_slcr_softc *sc = device_get_softc(dev);
	int rid;
	uint32_t bootmode;
	uint32_t pss_idcode;
	static char *bootdev_names[] = {
		"JTAG", "Quad-SPI", "NOR", "(3?)",
		"NAND", "SD Card", "(6?)", "(7?)"
	};

	/* Allow only one attach. */
	if (zy7_slcr_softc_p != NULL)
		return (ENXIO);

	sc->dev = dev;

	ZSLCR_LOCK_INIT(sc);

	/* Get memory resource. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
					     RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		return (ENOMEM);
	}

	/* Hook up cpu_reset. */
	zy7_slcr_softc_p = sc;
	zynq7_cpu_reset = zy7_slcr_cpu_reset;

	/* Read info and set sysctls. */
	bootmode = RD4(sc, ZY7_SLCR_BOOT_MODE);
	snprintf(zynq_bootmode, sizeof(zynq_bootmode),
		 "0x%x: boot device: %s", bootmode,
		 bootdev_names[bootmode & ZY7_SLCR_BOOT_MODE_BOOTDEV_MASK]);

	pss_idcode = RD4(sc, ZY7_SLCR_PSS_IDCODE);
	snprintf(zynq_pssid, sizeof(zynq_pssid),
		 "0x%x: manufacturer: 0x%x device: 0x%x "
		 "family: 0x%x sub-family: 0x%x rev: 0x%x",
		 pss_idcode,
		 (pss_idcode & ZY7_SLCR_PSS_IDCODE_MNFR_ID_MASK) >>
		 ZY7_SLCR_PSS_IDCODE_MNFR_ID_SHIFT,
		 (pss_idcode & ZY7_SLCR_PSS_IDCODE_DEVICE_MASK) >>
		 ZY7_SLCR_PSS_IDCODE_DEVICE_SHIFT,
		 (pss_idcode & ZY7_SLCR_PSS_IDCODE_FAMILY_MASK) >>
		 ZY7_SLCR_PSS_IDCODE_FAMILY_SHIFT,
		 (pss_idcode & ZY7_SLCR_PSS_IDCODE_SUB_FAMILY_MASK) >>
		 ZY7_SLCR_PSS_IDCODE_SUB_FAMILY_SHIFT,
		 (pss_idcode & ZY7_SLCR_PSS_IDCODE_REVISION_MASK) >>
		 ZY7_SLCR_PSS_IDCODE_REVISION_SHIFT);

	zynq_reboot_status = RD4(sc, ZY7_SLCR_REBOOT_STAT);

	/* Lock SLCR registers. */
	zy7_slcr_lock(sc);

	return (0);
}

static int
zy7_slcr_detach(device_t dev)
{
	struct zy7_slcr_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);

	/* Release memory resource. */
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
			     rman_get_rid(sc->mem_res), sc->mem_res);

	zy7_slcr_softc_p = NULL;
	zynq7_cpu_reset = NULL;

	ZSLCR_LOCK_DESTROY(sc);

	return (0);
}

static device_method_t zy7_slcr_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	zy7_slcr_probe),
	DEVMETHOD(device_attach, 	zy7_slcr_attach),
	DEVMETHOD(device_detach, 	zy7_slcr_detach),

	DEVMETHOD_END
};

static driver_t zy7_slcr_driver = {
	"zy7_slcr",
	zy7_slcr_methods,
	sizeof(struct zy7_slcr_softc),
};
static devclass_t zy7_slcr_devclass;

DRIVER_MODULE(zy7_slcr, simplebus, zy7_slcr_driver, zy7_slcr_devclass, 0, 0);
MODULE_VERSION(zy7_slcr, 1);
