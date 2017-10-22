/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/arm/broadcom/bcm2835/bcm2835_sdhci.c 252449 2013-07-01 06:32:56Z rpaulo $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <sys/kdb.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include <dev/sdhci/sdhci.h>
#include "sdhci_if.h"

#include "bcm2835_dma.h"
#include "bcm2835_vcbus.h"

#define	BCM2835_DEFAULT_SDHCI_FREQ	50

#define	BCM_SDHCI_BUFFER_SIZE		512

#ifdef DEBUG
#define dprintf(fmt, args...) do { printf("%s(): ", __func__);   \
    printf(fmt,##args); } while (0)
#else
#define dprintf(fmt, args...)
#endif

/* 
 * Arasan HC seems to have problem with Data CRC on lower frequencies.
 * Use this tunable to cap initialization sequence frequency at higher
 * value. Default is standard 400kHz
 */
static int bcm2835_sdhci_min_freq = 400000;
static int bcm2835_sdhci_hs = 1;
static int bcm2835_sdhci_pio_mode = 0;

TUNABLE_INT("hw.bcm2835.sdhci.min_freq", &bcm2835_sdhci_min_freq);
TUNABLE_INT("hw.bcm2835.sdhci.hs", &bcm2835_sdhci_hs);
TUNABLE_INT("hw.bcm2835.sdhci.pio_mode", &bcm2835_sdhci_pio_mode);

struct bcm_sdhci_dmamap_arg {
	bus_addr_t		sc_dma_busaddr;
};

struct bcm_sdhci_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource *	sc_mem_res;
	struct resource *	sc_irq_res;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	void *			sc_intrhand;
	struct mmc_request *	sc_req;
	struct mmc_data *	sc_data;
	uint32_t		sc_flags;
#define	LPC_SD_FLAGS_IGNORECRC		(1 << 0)
	int			sc_xfer_direction;
#define	DIRECTION_READ		0
#define	DIRECTION_WRITE		1
	int			sc_xfer_done;
	int			sc_bus_busy;
	struct sdhci_slot	sc_slot;
	int			sc_dma_inuse;
	int			sc_dma_ch;
	bus_dma_tag_t		sc_dma_tag;
	bus_dmamap_t		sc_dma_map;
	vm_paddr_t		sc_sdhci_buffer_phys;
};

static int bcm_sdhci_probe(device_t);
static int bcm_sdhci_attach(device_t);
static int bcm_sdhci_detach(device_t);
static void bcm_sdhci_intr(void *);

static int bcm_sdhci_get_ro(device_t, device_t);
static void bcm_sdhci_dma_intr(int ch, void *arg);

#define	bcm_sdhci_lock(_sc)						\
    mtx_lock(&_sc->sc_mtx);
#define	bcm_sdhci_unlock(_sc)						\
    mtx_unlock(&_sc->sc_mtx);

static void
bcm_dmamap_cb(void *arg, bus_dma_segment_t *segs,
	int nseg, int err)
{
        bus_addr_t *addr;

        if (err)
                return;

        addr = (bus_addr_t*)arg;
        *addr = segs[0].ds_addr;
}

static int
bcm_sdhci_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-sdhci"))
		return (ENXIO);

	device_set_desc(dev, "Broadcom 2708 SDHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
bcm_sdhci_attach(device_t dev)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	int rid, err;
	phandle_t node;
	pcell_t cell;
	int default_freq;

	sc->sc_dev = dev;
	sc->sc_req = NULL;
	err = 0;

	default_freq = BCM2835_DEFAULT_SDHCI_FREQ;
	node = ofw_bus_get_node(sc->sc_dev);
	if ((OF_getprop(node, "clock-frequency", &cell, sizeof(cell))) > 0)
		default_freq = (int)fdt32_to_cpu(cell)/1000000;

	dprintf("SDHCI frequency: %dMHz\n", default_freq);

	mtx_init(&sc->sc_mtx, "bcm sdhci", "sdhci", MTX_DEF);

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		err = ENXIO;
		goto fail;
	}

	sc->sc_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		err = ENXIO;
		goto fail;
	}

	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, bcm_sdhci_intr, sc, &sc->sc_intrhand))
	{
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		err = ENXIO;
		goto fail;
	}

	if (!bcm2835_sdhci_pio_mode)
		sc->sc_slot.opt = SDHCI_PLATFORM_TRANSFER;

	sc->sc_slot.caps = SDHCI_CAN_VDD_330 | SDHCI_CAN_VDD_180;
	if (bcm2835_sdhci_hs)
		sc->sc_slot.caps |= SDHCI_CAN_DO_HISPD;
	sc->sc_slot.caps |= (default_freq << SDHCI_CLOCK_BASE_SHIFT);
	sc->sc_slot.quirks = SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK 
		| SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
		| SDHCI_QUIRK_MISSING_CAPS;
 
	sdhci_init_slot(dev, &sc->sc_slot, 0);

	sc->sc_dma_ch = bcm_dma_allocate(BCM_DMA_CH_FAST1);
	if (sc->sc_dma_ch == BCM_DMA_CH_INVALID)
		sc->sc_dma_ch = bcm_dma_allocate(BCM_DMA_CH_FAST2);
	if (sc->sc_dma_ch == BCM_DMA_CH_INVALID)
		sc->sc_dma_ch = bcm_dma_allocate(BCM_DMA_CH_ANY);
	if (sc->sc_dma_ch == BCM_DMA_CH_INVALID)
		goto fail;

	bcm_dma_setup_intr(sc->sc_dma_ch, bcm_sdhci_dma_intr, sc);

	/* Allocate bus_dma resources. */
	err = bus_dma_tag_create(bus_get_dma_tag(dev),
	    1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL,
	    BCM_SDHCI_BUFFER_SIZE, 1, BCM_SDHCI_BUFFER_SIZE,
	    BUS_DMA_ALLOCNOW, NULL, NULL,
	    &sc->sc_dma_tag);

	if (err) {
		device_printf(dev, "failed allocate DMA tag");
		goto fail;
	}

	err = bus_dmamap_create(sc->sc_dma_tag, 0, &sc->sc_dma_map);
	if (err) {
		device_printf(dev, "bus_dmamap_create failed\n");
		goto fail;
	}

	sc->sc_sdhci_buffer_phys = BUS_SPACE_PHYSADDR(sc->sc_mem_res, 
	    SDHCI_BUFFER);

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	sdhci_start_slot(&sc->sc_slot);

	return (0);

fail:
	if (sc->sc_intrhand)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);

	return (err);
}

static int
bcm_sdhci_detach(device_t dev)
{

	return (EBUSY);
}

static void
bcm_sdhci_intr(void *arg)
{
	struct bcm_sdhci_softc *sc = arg;

	sdhci_generic_intr(&sc->sc_slot);
}

static int
bcm_sdhci_get_ro(device_t bus, device_t child)
{

	return (0);
}

static inline uint32_t
RD4(struct bcm_sdhci_softc *sc, bus_size_t off)
{
	uint32_t val = bus_space_read_4(sc->sc_bst, sc->sc_bsh, off);
	return val;
}

static inline void
WR4(struct bcm_sdhci_softc *sc, bus_size_t off, uint32_t val)
{
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, off, val);

	if ((off != SDHCI_BUFFER && off != SDHCI_INT_STATUS && off != SDHCI_CLOCK_CONTROL))
	{
		int timeout = 100000;
		while (val != bus_space_read_4(sc->sc_bst, sc->sc_bsh, off) 
		    && --timeout > 0)
			continue;

		if (timeout <= 0)
			printf("sdhci_brcm: writing 0x%X to reg 0x%X "
				"always gives 0x%X\n",
				val, (uint32_t)off, 
				bus_space_read_4(sc->sc_bst, sc->sc_bsh, off));
	}
}

static uint8_t
bcm_sdhci_read_1(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val = RD4(sc, off & ~3);

	return ((val >> (off & 3)*8) & 0xff);
}

static uint16_t
bcm_sdhci_read_2(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val = RD4(sc, off & ~3);

	return ((val >> (off & 3)*8) & 0xffff);
}

static uint32_t
bcm_sdhci_read_4(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);

	return RD4(sc, off);
}

static void
bcm_sdhci_read_multi_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t *data, bus_size_t count)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);

	bus_space_read_multi_4(sc->sc_bst, sc->sc_bsh, off, data, count);
}

static void
bcm_sdhci_write_1(device_t dev, struct sdhci_slot *slot, bus_size_t off, uint8_t val)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val32 = RD4(sc, off & ~3);
	val32 &= ~(0xff << (off & 3)*8);
	val32 |= (val << (off & 3)*8);
	WR4(sc, off & ~3, val32);
}

static void
bcm_sdhci_write_2(device_t dev, struct sdhci_slot *slot, bus_size_t off, uint16_t val)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	static uint32_t cmd_and_trandfer_mode;
	uint32_t val32;
	if (off == SDHCI_COMMAND_FLAGS)
		val32 = cmd_and_trandfer_mode;
	else
		val32 = RD4(sc, off & ~3);
	val32 &= ~(0xffff << (off & 3)*8);
	val32 |= (val << (off & 3)*8);
	if (off == SDHCI_TRANSFER_MODE)
		cmd_and_trandfer_mode = val32;
	else
		WR4(sc, off & ~3, val32);
}

static void
bcm_sdhci_write_4(device_t dev, struct sdhci_slot *slot, bus_size_t off, uint32_t val)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	WR4(sc, off, val);
}

static void
bcm_sdhci_write_multi_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t *data, bus_size_t count)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);

	bus_space_write_multi_4(sc->sc_bst, sc->sc_bsh, off, data, count);
}

static uint32_t
bcm_sdhci_min_freq(device_t dev, struct sdhci_slot *slot)
{

	return bcm2835_sdhci_min_freq;
}

static void
bcm_sdhci_dma_intr(int ch, void *arg)
{
	struct bcm_sdhci_softc *sc = (struct bcm_sdhci_softc *)arg;
	struct sdhci_slot *slot = &sc->sc_slot;
	uint32_t reg, mask;
	bus_addr_t pmem;
	vm_paddr_t pdst, psrc;
	size_t len;
	int left, sync_op;

	mtx_lock(&slot->mtx);

	len = bcm_dma_length(sc->sc_dma_ch);
	if (slot->curcmd->data->flags & MMC_DATA_READ) {
		sync_op = BUS_DMASYNC_POSTREAD;
		mask = SDHCI_INT_DATA_AVAIL;
	} else {
		sync_op = BUS_DMASYNC_POSTWRITE;
		mask = SDHCI_INT_SPACE_AVAIL;
	}
	bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map, sync_op);
	bus_dmamap_unload(sc->sc_dma_tag, sc->sc_dma_map);

	slot->offset += len;
	sc->sc_dma_inuse = 0;

	left = min(BCM_SDHCI_BUFFER_SIZE,
	    slot->curcmd->data->len - slot->offset);

	/* DATA END? */
	reg = bcm_sdhci_read_4(slot->bus, slot, SDHCI_INT_STATUS);

	if (reg & SDHCI_INT_DATA_END) {
		/* ACK for all outstanding interrupts */
		bcm_sdhci_write_4(slot->bus, slot, SDHCI_INT_STATUS, reg);

		/* enable INT */
		slot->intmask |= SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL
		    | SDHCI_INT_DATA_END;
		bcm_sdhci_write_4(slot->bus, slot, SDHCI_SIGNAL_ENABLE,
		    slot->intmask);

		/* finish this data */
		sdhci_finish_data(slot);
	} 
	else {
		/* already available? */
		if (reg & mask) {
			sc->sc_dma_inuse = 1;

			/* ACK for DATA_AVAIL or SPACE_AVAIL */
			bcm_sdhci_write_4(slot->bus, slot,
			    SDHCI_INT_STATUS, mask);

			/* continue next DMA transfer */
			bus_dmamap_load(sc->sc_dma_tag, sc->sc_dma_map, 
			    (uint8_t *)slot->curcmd->data->data + 
			    slot->offset, left, bcm_dmamap_cb, &pmem, 0);
			if (slot->curcmd->data->flags & MMC_DATA_READ) {
				psrc = sc->sc_sdhci_buffer_phys;
				pdst = pmem;
				sync_op = BUS_DMASYNC_PREREAD;
			} else {
				psrc = pmem;
				pdst = sc->sc_sdhci_buffer_phys;
				sync_op = BUS_DMASYNC_PREWRITE;
			}
			bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map, sync_op);
			if (bcm_dma_start(sc->sc_dma_ch, psrc, pdst, left)) {
				/* XXX stop xfer, other error recovery? */
				device_printf(sc->sc_dev, "failed DMA start\n");
			}
		} else {
			/* wait for next data by INT */

			/* enable INT */
			slot->intmask |= SDHCI_INT_DATA_AVAIL |
			    SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_END;
			bcm_sdhci_write_4(slot->bus, slot, SDHCI_SIGNAL_ENABLE,
			    slot->intmask);
		}
	}

	mtx_unlock(&slot->mtx);
}

static void
bcm_sdhci_read_dma(struct sdhci_slot *slot)
{
	struct bcm_sdhci_softc *sc = device_get_softc(slot->bus);
	size_t left;
	bus_addr_t paddr;

	if (sc->sc_dma_inuse) {
		device_printf(sc->sc_dev, "DMA in use\n");
		return;
	}

	sc->sc_dma_inuse = 1;

	left = min(BCM_SDHCI_BUFFER_SIZE,
	    slot->curcmd->data->len - slot->offset);

	KASSERT((left & 3) == 0,
	    ("%s: len = %d, not word-aligned", __func__, left));

	bcm_dma_setup_src(sc->sc_dma_ch, BCM_DMA_DREQ_EMMC,
	    BCM_DMA_SAME_ADDR, BCM_DMA_32BIT); 
	bcm_dma_setup_dst(sc->sc_dma_ch, BCM_DMA_DREQ_NONE,
	    BCM_DMA_INC_ADDR,
	    (left & 0xf) ? BCM_DMA_32BIT : BCM_DMA_128BIT);

	bus_dmamap_load(sc->sc_dma_tag, sc->sc_dma_map, 
	    (uint8_t *)slot->curcmd->data->data + slot->offset, left, 
	    bcm_dmamap_cb, &paddr, 0);

	bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map,
	    BUS_DMASYNC_PREREAD);

	/* DMA start */
	if (bcm_dma_start(sc->sc_dma_ch, sc->sc_sdhci_buffer_phys,
	    paddr, left) != 0)
		device_printf(sc->sc_dev, "failed DMA start\n");
}

static void
bcm_sdhci_write_dma(struct sdhci_slot *slot)
{
	struct bcm_sdhci_softc *sc = device_get_softc(slot->bus);
	size_t left;
	bus_addr_t paddr;

	if (sc->sc_dma_inuse) {
		device_printf(sc->sc_dev, "DMA in use\n");
		return;
	}

	sc->sc_dma_inuse = 1;

	left = min(BCM_SDHCI_BUFFER_SIZE,
	    slot->curcmd->data->len - slot->offset);

	KASSERT((left & 3) == 0,
	    ("%s: len = %d, not word-aligned", __func__, left));

	bus_dmamap_load(sc->sc_dma_tag, sc->sc_dma_map,
	    (uint8_t *)slot->curcmd->data->data + slot->offset, left, 
	    bcm_dmamap_cb, &paddr, 0);

	bcm_dma_setup_src(sc->sc_dma_ch, BCM_DMA_DREQ_NONE,
	    BCM_DMA_INC_ADDR,
	    (left & 0xf) ? BCM_DMA_32BIT : BCM_DMA_128BIT);
	bcm_dma_setup_dst(sc->sc_dma_ch, BCM_DMA_DREQ_EMMC,
	    BCM_DMA_SAME_ADDR, BCM_DMA_32BIT);

	bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map,
	    BUS_DMASYNC_PREWRITE);

	/* DMA start */
	if (bcm_dma_start(sc->sc_dma_ch, paddr,
	    sc->sc_sdhci_buffer_phys, left) != 0)
		device_printf(sc->sc_dev, "failed DMA start\n");
}

static int
bcm_sdhci_will_handle_transfer(device_t dev, struct sdhci_slot *slot)
{
	size_t left;

	/*
	 * Do not use DMA for transfers less than block size or with a length
	 * that is not a multiple of four.
	 */
	left = min(BCM_DMA_BLOCK_SIZE,
	    slot->curcmd->data->len - slot->offset);
	if (left < BCM_DMA_BLOCK_SIZE)
		return (0);
	if (left & 0x03)
		return (0);

	return (1);
}

static void
bcm_sdhci_start_transfer(device_t dev, struct sdhci_slot *slot,
    uint32_t *intmask)
{

	/* Disable INT */
	slot->intmask &= ~(SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_END);
	bcm_sdhci_write_4(dev, slot, SDHCI_SIGNAL_ENABLE, slot->intmask);

	/* DMA transfer FIFO 1KB */
	if (slot->curcmd->data->flags & MMC_DATA_READ)
		bcm_sdhci_read_dma(slot);
	else
		bcm_sdhci_write_dma(slot);
}

static void
bcm_sdhci_finish_transfer(device_t dev, struct sdhci_slot *slot)
{

	sdhci_finish_data(slot);
}

static device_method_t bcm_sdhci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_sdhci_probe),
	DEVMETHOD(device_attach,	bcm_sdhci_attach),
	DEVMETHOD(device_detach,	bcm_sdhci_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sdhci_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_generic_write_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* MMC bridge interface */
	DEVMETHOD(mmcbr_update_ios,	sdhci_generic_update_ios),
	DEVMETHOD(mmcbr_request,	sdhci_generic_request),
	DEVMETHOD(mmcbr_get_ro,		bcm_sdhci_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	sdhci_generic_acquire_host),
	DEVMETHOD(mmcbr_release_host,	sdhci_generic_release_host),

	DEVMETHOD(sdhci_min_freq,	bcm_sdhci_min_freq),
	/* Platform transfer methods */
	DEVMETHOD(sdhci_platform_will_handle,		bcm_sdhci_will_handle_transfer),
	DEVMETHOD(sdhci_platform_start_transfer,	bcm_sdhci_start_transfer),
	DEVMETHOD(sdhci_platform_finish_transfer,	bcm_sdhci_finish_transfer),
	/* SDHCI registers accessors */
	DEVMETHOD(sdhci_read_1,		bcm_sdhci_read_1),
	DEVMETHOD(sdhci_read_2,		bcm_sdhci_read_2),
	DEVMETHOD(sdhci_read_4,		bcm_sdhci_read_4),
	DEVMETHOD(sdhci_read_multi_4,	bcm_sdhci_read_multi_4),
	DEVMETHOD(sdhci_write_1,	bcm_sdhci_write_1),
	DEVMETHOD(sdhci_write_2,	bcm_sdhci_write_2),
	DEVMETHOD(sdhci_write_4,	bcm_sdhci_write_4),
	DEVMETHOD(sdhci_write_multi_4,	bcm_sdhci_write_multi_4),

	{ 0, 0 }
};

static devclass_t bcm_sdhci_devclass;

static driver_t bcm_sdhci_driver = {
	"sdhci_bcm",
	bcm_sdhci_methods,
	sizeof(struct bcm_sdhci_softc),
};

DRIVER_MODULE(sdhci_bcm, simplebus, bcm_sdhci_driver, bcm_sdhci_devclass, 0, 0);
MODULE_DEPEND(sdhci_bcm, sdhci, 1, 1, 1);
