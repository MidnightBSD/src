/*-
 * Copyright (c) 2008 Alexander Motin <mav@FreeBSD.org>
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
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/sdhci/sdhci.c 254512 2013-08-19 05:48:42Z rpaulo $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include "mmcbr_if.h"
#include "sdhci.h"
#include "sdhci_if.h"

struct sdhci_softc;

struct sdhci_softc {
	device_t	dev;		/* Controller device */
	struct resource *irq_res;	/* IRQ resource */
	int 		irq_rid;
	void 		*intrhand;	/* Interrupt handle */

	int		num_slots;	/* Number of slots on this controller */
	struct sdhci_slot slots[6];
};

static SYSCTL_NODE(_hw, OID_AUTO, sdhci, CTLFLAG_RD, 0, "sdhci driver");

int	sdhci_debug = 0;
TUNABLE_INT("hw.sdhci.debug", &sdhci_debug);
SYSCTL_INT(_hw_sdhci, OID_AUTO, debug, CTLFLAG_RW, &sdhci_debug, 0, "Debug level");

#define RD1(slot, off)	SDHCI_READ_1((slot)->bus, (slot), (off))
#define RD2(slot, off)	SDHCI_READ_2((slot)->bus, (slot), (off))
#define RD4(slot, off)	SDHCI_READ_4((slot)->bus, (slot), (off))
#define RD_MULTI_4(slot, off, ptr, count)	\
    SDHCI_READ_MULTI_4((slot)->bus, (slot), (off), (ptr), (count))

#define WR1(slot, off, val)	SDHCI_WRITE_1((slot)->bus, (slot), (off), (val))
#define WR2(slot, off, val)	SDHCI_WRITE_2((slot)->bus, (slot), (off), (val))
#define WR4(slot, off, val)	SDHCI_WRITE_4((slot)->bus, (slot), (off), (val))
#define WR_MULTI_4(slot, off, ptr, count)	\
    SDHCI_WRITE_MULTI_4((slot)->bus, (slot), (off), (ptr), (count))

static void sdhci_set_clock(struct sdhci_slot *slot, uint32_t clock);
static void sdhci_start(struct sdhci_slot *slot);
static void sdhci_start_data(struct sdhci_slot *slot, struct mmc_data *data);

static void sdhci_card_task(void *, int);

/* helper routines */
#define SDHCI_LOCK(_slot)		mtx_lock(&(_slot)->mtx)
#define	SDHCI_UNLOCK(_slot)		mtx_unlock(&(_slot)->mtx)
#define SDHCI_LOCK_INIT(_slot) \
	mtx_init(&_slot->mtx, "SD slot mtx", "sdhci", MTX_DEF)
#define SDHCI_LOCK_DESTROY(_slot)	mtx_destroy(&_slot->mtx);
#define SDHCI_ASSERT_LOCKED(_slot)	mtx_assert(&_slot->mtx, MA_OWNED);
#define SDHCI_ASSERT_UNLOCKED(_slot)	mtx_assert(&_slot->mtx, MA_NOTOWNED);

#define	SDHCI_DEFAULT_MAX_FREQ	50

#define	SDHCI_200_MAX_DIVIDER	256
#define	SDHCI_300_MAX_DIVIDER	2046

static void
sdhci_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0) {
		printf("getaddr: error %d\n", error);
		return;
	}
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
slot_printf(struct sdhci_slot *slot, const char * fmt, ...)
{
	va_list ap;
	int retval;

    	retval = printf("%s-slot%d: ",
	    device_get_nameunit(slot->bus), slot->num);

	va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	va_end(ap);
	return (retval);
}

static void
sdhci_dumpregs(struct sdhci_slot *slot)
{
	slot_printf(slot,
	    "============== REGISTER DUMP ==============\n");

	slot_printf(slot, "Sys addr: 0x%08x | Version:  0x%08x\n",
	    RD4(slot, SDHCI_DMA_ADDRESS), RD2(slot, SDHCI_HOST_VERSION));
	slot_printf(slot, "Blk size: 0x%08x | Blk cnt:  0x%08x\n",
	    RD2(slot, SDHCI_BLOCK_SIZE), RD2(slot, SDHCI_BLOCK_COUNT));
	slot_printf(slot, "Argument: 0x%08x | Trn mode: 0x%08x\n",
	    RD4(slot, SDHCI_ARGUMENT), RD2(slot, SDHCI_TRANSFER_MODE));
	slot_printf(slot, "Present:  0x%08x | Host ctl: 0x%08x\n",
	    RD4(slot, SDHCI_PRESENT_STATE), RD1(slot, SDHCI_HOST_CONTROL));
	slot_printf(slot, "Power:    0x%08x | Blk gap:  0x%08x\n",
	    RD1(slot, SDHCI_POWER_CONTROL), RD1(slot, SDHCI_BLOCK_GAP_CONTROL));
	slot_printf(slot, "Wake-up:  0x%08x | Clock:    0x%08x\n",
	    RD1(slot, SDHCI_WAKE_UP_CONTROL), RD2(slot, SDHCI_CLOCK_CONTROL));
	slot_printf(slot, "Timeout:  0x%08x | Int stat: 0x%08x\n",
	    RD1(slot, SDHCI_TIMEOUT_CONTROL), RD4(slot, SDHCI_INT_STATUS));
	slot_printf(slot, "Int enab: 0x%08x | Sig enab: 0x%08x\n",
	    RD4(slot, SDHCI_INT_ENABLE), RD4(slot, SDHCI_SIGNAL_ENABLE));
	slot_printf(slot, "AC12 err: 0x%08x | Slot int: 0x%08x\n",
	    RD2(slot, SDHCI_ACMD12_ERR), RD2(slot, SDHCI_SLOT_INT_STATUS));
	slot_printf(slot, "Caps:     0x%08x | Max curr: 0x%08x\n",
	    RD4(slot, SDHCI_CAPABILITIES), RD4(slot, SDHCI_MAX_CURRENT));

	slot_printf(slot,
	    "===========================================\n");
}

static void
sdhci_reset(struct sdhci_slot *slot, uint8_t mask)
{
	int timeout;
	uint8_t res;

	if (slot->quirks & SDHCI_QUIRK_NO_CARD_NO_RESET) {
		if (!(RD4(slot, SDHCI_PRESENT_STATE) &
			SDHCI_CARD_PRESENT))
			return;
	}

	/* Some controllers need this kick or reset won't work. */
	if ((mask & SDHCI_RESET_ALL) == 0 &&
	    (slot->quirks & SDHCI_QUIRK_CLOCK_BEFORE_RESET)) {
		uint32_t clock;

		/* This is to force an update */
		clock = slot->clock;
		slot->clock = 0;
		sdhci_set_clock(slot, clock);
	}

	WR1(slot, SDHCI_SOFTWARE_RESET, mask);

	if (mask & SDHCI_RESET_ALL) {
		slot->clock = 0;
		slot->power = 0;
	}

	/* Wait max 100 ms */
	timeout = 100;
	/* Controller clears the bits when it's done */
	while ((res = RD1(slot, SDHCI_SOFTWARE_RESET)) & mask) {
		if (timeout == 0) {
			slot_printf(slot,
			    "Reset 0x%x never completed - 0x%x.\n",
			    (int)mask, (int)res);
			sdhci_dumpregs(slot);
			return;
		}
		timeout--;
		DELAY(1000);
	}
}

static void
sdhci_init(struct sdhci_slot *slot)
{

	sdhci_reset(slot, SDHCI_RESET_ALL);

	/* Enable interrupts. */
	slot->intmask = SDHCI_INT_BUS_POWER | SDHCI_INT_DATA_END_BIT |
	    SDHCI_INT_DATA_CRC | SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_INDEX |
	    SDHCI_INT_END_BIT | SDHCI_INT_CRC | SDHCI_INT_TIMEOUT |
	    SDHCI_INT_CARD_REMOVE | SDHCI_INT_CARD_INSERT |
	    SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL |
	    SDHCI_INT_DMA_END | SDHCI_INT_DATA_END | SDHCI_INT_RESPONSE |
	    SDHCI_INT_ACMD12ERR;
	WR4(slot, SDHCI_INT_ENABLE, slot->intmask);
	WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask);
}

static void
sdhci_set_clock(struct sdhci_slot *slot, uint32_t clock)
{
	uint32_t res;
	uint16_t clk;
	uint16_t div;
	int timeout;

	if (clock == slot->clock)
		return;
	slot->clock = clock;

	/* Turn off the clock. */
	WR2(slot, SDHCI_CLOCK_CONTROL, 0);
	/* If no clock requested - left it so. */
	if (clock == 0)
		return;

	/* Recalculate timeout clock frequency based on the new sd clock. */
	if (slot->quirks & SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK)
		slot->timeout_clk = slot->clock / 1000;

	if (slot->version < SDHCI_SPEC_300) {
		/* Looking for highest freq <= clock. */
		res = slot->max_clk;
		for (div = 1; div < SDHCI_200_MAX_DIVIDER; div <<= 1) {
			if (res <= clock)
				break;
			res >>= 1;
		}
		/* Divider 1:1 is 0x00, 2:1 is 0x01, 256:1 is 0x80 ... */
		div >>= 1;
	}
	else {
		/* Version 3.0 divisors are multiples of two up to 1023*2 */
		if (clock >= slot->max_clk)
			div = 0;
		else {
			for (div = 2; div < SDHCI_300_MAX_DIVIDER; div += 2) { 
				if ((slot->max_clk / div) <= clock) 
					break;
			}
		}
		div >>= 1;
	}

	if (bootverbose || sdhci_debug)
		slot_printf(slot, "Divider %d for freq %d (max %d)\n", 
			div, clock, slot->max_clk);

	/* Now we have got divider, set it. */
	clk = (div & SDHCI_DIVIDER_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div >> SDHCI_DIVIDER_MASK_LEN) & SDHCI_DIVIDER_HI_MASK)
		<< SDHCI_DIVIDER_HI_SHIFT;

	WR2(slot, SDHCI_CLOCK_CONTROL, clk);
	/* Enable clock. */
	clk |= SDHCI_CLOCK_INT_EN;
	WR2(slot, SDHCI_CLOCK_CONTROL, clk);
	/* Wait up to 10 ms until it stabilize. */
	timeout = 10;
	while (!((clk = RD2(slot, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			slot_printf(slot, 
			    "Internal clock never stabilised.\n");
			sdhci_dumpregs(slot);
			return;
		}
		timeout--;
		DELAY(1000);
	}
	/* Pass clock signal to the bus. */
	clk |= SDHCI_CLOCK_CARD_EN;
	WR2(slot, SDHCI_CLOCK_CONTROL, clk);
}

static void
sdhci_set_power(struct sdhci_slot *slot, u_char power)
{
	uint8_t pwr;

	if (slot->power == power)
		return;

	slot->power = power;

	/* Turn off the power. */
	pwr = 0;
	WR1(slot, SDHCI_POWER_CONTROL, pwr);
	/* If power down requested - left it so. */
	if (power == 0)
		return;
	/* Set voltage. */
	switch (1 << power) {
	case MMC_OCR_LOW_VOLTAGE:
		pwr |= SDHCI_POWER_180;
		break;
	case MMC_OCR_290_300:
	case MMC_OCR_300_310:
		pwr |= SDHCI_POWER_300;
		break;
	case MMC_OCR_320_330:
	case MMC_OCR_330_340:
		pwr |= SDHCI_POWER_330;
		break;
	}
	WR1(slot, SDHCI_POWER_CONTROL, pwr);
	/* Turn on the power. */
	pwr |= SDHCI_POWER_ON;
	WR1(slot, SDHCI_POWER_CONTROL, pwr);
}

static void
sdhci_read_block_pio(struct sdhci_slot *slot)
{
	uint32_t data;
	char *buffer;
	size_t left;

	buffer = slot->curcmd->data->data;
	buffer += slot->offset;
	/* Transfer one block at a time. */
	left = min(512, slot->curcmd->data->len - slot->offset);
	slot->offset += left;

	/* If we are too fast, broken controllers return zeroes. */
	if (slot->quirks & SDHCI_QUIRK_BROKEN_TIMINGS)
		DELAY(10);
	/* Handle unaligned and aligned buffer cases. */
	if ((intptr_t)buffer & 3) {
		while (left > 3) {
			data = RD4(slot, SDHCI_BUFFER);
			buffer[0] = data;
			buffer[1] = (data >> 8);
			buffer[2] = (data >> 16);
			buffer[3] = (data >> 24);
			buffer += 4;
			left -= 4;
		}
	} else {
		RD_MULTI_4(slot, SDHCI_BUFFER,
		    (uint32_t *)buffer, left >> 2);
		left &= 3;
	}
	/* Handle uneven size case. */
	if (left > 0) {
		data = RD4(slot, SDHCI_BUFFER);
		while (left > 0) {
			*(buffer++) = data;
			data >>= 8;
			left--;
		}
	}
}

static void
sdhci_write_block_pio(struct sdhci_slot *slot)
{
	uint32_t data = 0;
	char *buffer;
	size_t left;

	buffer = slot->curcmd->data->data;
	buffer += slot->offset;
	/* Transfer one block at a time. */
	left = min(512, slot->curcmd->data->len - slot->offset);
	slot->offset += left;

	/* Handle unaligned and aligned buffer cases. */
	if ((intptr_t)buffer & 3) {
		while (left > 3) {
			data = buffer[0] +
			    (buffer[1] << 8) +
			    (buffer[2] << 16) +
			    (buffer[3] << 24);
			left -= 4;
			buffer += 4;
			WR4(slot, SDHCI_BUFFER, data);
		}
	} else {
		WR_MULTI_4(slot, SDHCI_BUFFER,
		    (uint32_t *)buffer, left >> 2);
		left &= 3;
	}
	/* Handle uneven size case. */
	if (left > 0) {
		while (left > 0) {
			data <<= 8;
			data += *(buffer++);
			left--;
		}
		WR4(slot, SDHCI_BUFFER, data);
	}
}

static void
sdhci_transfer_pio(struct sdhci_slot *slot)
{

	/* Read as many blocks as possible. */
	if (slot->curcmd->data->flags & MMC_DATA_READ) {
		while (RD4(slot, SDHCI_PRESENT_STATE) &
		    SDHCI_DATA_AVAILABLE) {
			sdhci_read_block_pio(slot);
			if (slot->offset >= slot->curcmd->data->len)
				break;
		}
	} else {
		while (RD4(slot, SDHCI_PRESENT_STATE) &
		    SDHCI_SPACE_AVAILABLE) {
			sdhci_write_block_pio(slot);
			if (slot->offset >= slot->curcmd->data->len)
				break;
		}
	}
}

static void 
sdhci_card_delay(void *arg)
{
	struct sdhci_slot *slot = arg;

	taskqueue_enqueue(taskqueue_swi_giant, &slot->card_task);
}
 
static void
sdhci_card_task(void *arg, int pending)
{
	struct sdhci_slot *slot = arg;

	SDHCI_LOCK(slot);
	if (RD4(slot, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT) {
		if (slot->dev == NULL) {
			/* If card is present - attach mmc bus. */
			slot->dev = device_add_child(slot->bus, "mmc", -1);
			device_set_ivars(slot->dev, slot);
			SDHCI_UNLOCK(slot);
			device_probe_and_attach(slot->dev);
		} else
			SDHCI_UNLOCK(slot);
	} else {
		if (slot->dev != NULL) {
			/* If no card present - detach mmc bus. */
			device_t d = slot->dev;
			slot->dev = NULL;
			SDHCI_UNLOCK(slot);
			device_delete_child(slot->bus, d);
		} else
			SDHCI_UNLOCK(slot);
	}
}

int
sdhci_init_slot(device_t dev, struct sdhci_slot *slot, int num)
{
	uint32_t caps, freq;
	int err;

	SDHCI_LOCK_INIT(slot);
	slot->num = num;
	slot->bus = dev;

	/* Allocate DMA tag. */
	err = bus_dma_tag_create(bus_get_dma_tag(dev),
	    DMA_BLOCK_SIZE, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL,
	    DMA_BLOCK_SIZE, 1, DMA_BLOCK_SIZE,
	    BUS_DMA_ALLOCNOW, NULL, NULL,
	    &slot->dmatag);
	if (err != 0) {
		device_printf(dev, "Can't create DMA tag\n");
		SDHCI_LOCK_DESTROY(slot);
		return (err);
	}
	/* Allocate DMA memory. */
	err = bus_dmamem_alloc(slot->dmatag, (void **)&slot->dmamem,
	    BUS_DMA_NOWAIT, &slot->dmamap);
	if (err != 0) {
		device_printf(dev, "Can't alloc DMA memory\n");
		SDHCI_LOCK_DESTROY(slot);
		return (err);
	}
	/* Map the memory. */
	err = bus_dmamap_load(slot->dmatag, slot->dmamap,
	    (void *)slot->dmamem, DMA_BLOCK_SIZE,
	    sdhci_getaddr, &slot->paddr, 0);
	if (err != 0 || slot->paddr == 0) {
		device_printf(dev, "Can't load DMA memory\n");
		SDHCI_LOCK_DESTROY(slot);
		if(err)
			return (err);
		else
			return (EFAULT);
	}

	/* Initialize slot. */
	sdhci_init(slot);
	slot->version = (RD2(slot, SDHCI_HOST_VERSION) 
		>> SDHCI_SPEC_VER_SHIFT) & SDHCI_SPEC_VER_MASK;
	if (slot->quirks & SDHCI_QUIRK_MISSING_CAPS)
		caps = slot->caps;
	else
		caps = RD4(slot, SDHCI_CAPABILITIES);
	/* Calculate base clock frequency. */
	if (slot->version >= SDHCI_SPEC_300)
		freq = (caps & SDHCI_CLOCK_V3_BASE_MASK) >>
		    SDHCI_CLOCK_BASE_SHIFT;
	else	
		freq = (caps & SDHCI_CLOCK_BASE_MASK) >>
		    SDHCI_CLOCK_BASE_SHIFT;
	if (freq != 0)
		slot->max_clk = freq * 1000000;
	/*
	 * If the frequency wasn't in the capabilities and the hardware driver
	 * hasn't already set max_clk we're probably not going to work right
	 * with an assumption, so complain about it.
	 */
	if (slot->max_clk == 0) {
		slot->max_clk = SDHCI_DEFAULT_MAX_FREQ * 1000000;
		device_printf(dev, "Hardware doesn't specify base clock "
		    "frequency, using %dMHz as default.\n", SDHCI_DEFAULT_MAX_FREQ);
	}
	/* Calculate timeout clock frequency. */
	if (slot->quirks & SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK) {
		slot->timeout_clk = slot->max_clk / 1000;
	} else {
		slot->timeout_clk =
			(caps & SDHCI_TIMEOUT_CLK_MASK) >> SDHCI_TIMEOUT_CLK_SHIFT;
		if (caps & SDHCI_TIMEOUT_CLK_UNIT)
			slot->timeout_clk *= 1000;
	}
	/*
	 * If the frequency wasn't in the capabilities and the hardware driver
	 * hasn't already set timeout_clk we'll probably work okay using the
	 * max timeout, but still mention it.
	 */
	if (slot->timeout_clk == 0) {
		device_printf(dev, "Hardware doesn't specify timeout clock "
		    "frequency, setting BROKEN_TIMEOUT quirk.\n");
		slot->quirks |= SDHCI_QUIRK_BROKEN_TIMEOUT_VAL;
	}

	slot->host.f_min = SDHCI_MIN_FREQ(slot->bus, slot);
	slot->host.f_max = slot->max_clk;
	slot->host.host_ocr = 0;
	if (caps & SDHCI_CAN_VDD_330)
	    slot->host.host_ocr |= MMC_OCR_320_330 | MMC_OCR_330_340;
	if (caps & SDHCI_CAN_VDD_300)
	    slot->host.host_ocr |= MMC_OCR_290_300 | MMC_OCR_300_310;
	if (caps & SDHCI_CAN_VDD_180)
	    slot->host.host_ocr |= MMC_OCR_LOW_VOLTAGE;
	if (slot->host.host_ocr == 0) {
		device_printf(dev, "Hardware doesn't report any "
		    "support voltages.\n");
	}
	slot->host.caps = MMC_CAP_4_BIT_DATA;
	if (caps & SDHCI_CAN_DO_HISPD)
		slot->host.caps |= MMC_CAP_HSPEED;
	/* Decide if we have usable DMA. */
	if (caps & SDHCI_CAN_DO_DMA)
		slot->opt |= SDHCI_HAVE_DMA;

	if (slot->quirks & SDHCI_QUIRK_BROKEN_DMA)
		slot->opt &= ~SDHCI_HAVE_DMA;
	if (slot->quirks & SDHCI_QUIRK_FORCE_DMA)
		slot->opt |= SDHCI_HAVE_DMA;

	/* 
	 * Use platform-provided transfer backend
	 * with PIO as a fallback mechanism
	 */
	if (slot->opt & SDHCI_PLATFORM_TRANSFER)
		slot->opt &= ~SDHCI_HAVE_DMA;

	if (bootverbose || sdhci_debug) {
		slot_printf(slot, "%uMHz%s 4bits%s%s%s %s\n",
		    slot->max_clk / 1000000,
		    (caps & SDHCI_CAN_DO_HISPD) ? " HS" : "",
		    (caps & SDHCI_CAN_VDD_330) ? " 3.3V" : "",
		    (caps & SDHCI_CAN_VDD_300) ? " 3.0V" : "",
		    (caps & SDHCI_CAN_VDD_180) ? " 1.8V" : "",
		    (slot->opt & SDHCI_HAVE_DMA) ? "DMA" : "PIO");
		sdhci_dumpregs(slot);
	}
	
	TASK_INIT(&slot->card_task, 0, sdhci_card_task, slot);
	callout_init(&slot->card_callout, 1);
	return (0);
}

void
sdhci_start_slot(struct sdhci_slot *slot)
{
	sdhci_card_task(slot, 0);
}

int
sdhci_cleanup_slot(struct sdhci_slot *slot)
{
	device_t d;

	callout_drain(&slot->card_callout);
	taskqueue_drain(taskqueue_swi_giant, &slot->card_task);

	SDHCI_LOCK(slot);
	d = slot->dev;
	slot->dev = NULL;
	SDHCI_UNLOCK(slot);
	if (d != NULL)
		device_delete_child(slot->bus, d);

	SDHCI_LOCK(slot);
	sdhci_reset(slot, SDHCI_RESET_ALL);
	SDHCI_UNLOCK(slot);
	bus_dmamap_unload(slot->dmatag, slot->dmamap);
	bus_dmamem_free(slot->dmatag, slot->dmamem, slot->dmamap);
	bus_dma_tag_destroy(slot->dmatag);

	SDHCI_LOCK_DESTROY(slot);

	return (0);
}

int
sdhci_generic_suspend(struct sdhci_slot *slot)
{
	sdhci_reset(slot, SDHCI_RESET_ALL);

	return (0);
}

int
sdhci_generic_resume(struct sdhci_slot *slot)
{
	sdhci_init(slot);

	return (0);
}

uint32_t
sdhci_generic_min_freq(device_t brdev, struct sdhci_slot *slot)
{
	if (slot->version >= SDHCI_SPEC_300)
		return (slot->max_clk / SDHCI_300_MAX_DIVIDER);
	else
		return (slot->max_clk / SDHCI_200_MAX_DIVIDER);
}

int
sdhci_generic_update_ios(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);
	struct mmc_ios *ios = &slot->host.ios;

	SDHCI_LOCK(slot);
	/* Do full reset on bus power down to clear from any state. */
	if (ios->power_mode == power_off) {
		WR4(slot, SDHCI_SIGNAL_ENABLE, 0);
		sdhci_init(slot);
	}
	/* Configure the bus. */
	sdhci_set_clock(slot, ios->clock);
	sdhci_set_power(slot, (ios->power_mode == power_off)?0:ios->vdd);
	if (ios->bus_width == bus_width_4)
		slot->hostctrl |= SDHCI_CTRL_4BITBUS;
	else
		slot->hostctrl &= ~SDHCI_CTRL_4BITBUS;
	if (ios->timing == bus_timing_hs)
		slot->hostctrl |= SDHCI_CTRL_HISPD;
	else
		slot->hostctrl &= ~SDHCI_CTRL_HISPD;
	WR1(slot, SDHCI_HOST_CONTROL, slot->hostctrl);
	/* Some controllers like reset after bus changes. */
	if(slot->quirks & SDHCI_QUIRK_RESET_ON_IOS)
		sdhci_reset(slot, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	SDHCI_UNLOCK(slot);
	return (0);
}

static void
sdhci_set_transfer_mode(struct sdhci_slot *slot,
	struct mmc_data *data)
{
	uint16_t mode;

	if (data == NULL)
		return;

	mode = SDHCI_TRNS_BLK_CNT_EN;
	if (data->len > 512)
		mode |= SDHCI_TRNS_MULTI;
	if (data->flags & MMC_DATA_READ)
		mode |= SDHCI_TRNS_READ;
	if (slot->req->stop)
		mode |= SDHCI_TRNS_ACMD12;
	if (slot->flags & SDHCI_USE_DMA)
		mode |= SDHCI_TRNS_DMA;

	WR2(slot, SDHCI_TRANSFER_MODE, mode);
}

static void
sdhci_start_command(struct sdhci_slot *slot, struct mmc_command *cmd)
{
	struct mmc_request *req = slot->req;
	int flags, timeout;
	uint32_t mask, state;

	slot->curcmd = cmd;
	slot->cmd_done = 0;

	cmd->error = MMC_ERR_NONE;

	/* This flags combination is not supported by controller. */
	if ((cmd->flags & MMC_RSP_136) && (cmd->flags & MMC_RSP_BUSY)) {
		slot_printf(slot, "Unsupported response type!\n");
		cmd->error = MMC_ERR_FAILED;
		slot->req = NULL;
		slot->curcmd = NULL;
		req->done(req);
		return;
	}

	/* Read controller present state. */
	state = RD4(slot, SDHCI_PRESENT_STATE);
	/* Do not issue command if there is no card, clock or power.
	 * Controller will not detect timeout without clock active. */
	if ((state & SDHCI_CARD_PRESENT) == 0 ||
	    slot->power == 0 ||
	    slot->clock == 0) {
		cmd->error = MMC_ERR_FAILED;
		slot->req = NULL;
		slot->curcmd = NULL;
		req->done(req);
		return;
	}
	/* Always wait for free CMD bus. */
	mask = SDHCI_CMD_INHIBIT;
	/* Wait for free DAT if we have data or busy signal. */
	if (cmd->data || (cmd->flags & MMC_RSP_BUSY))
		mask |= SDHCI_DAT_INHIBIT;
	/* We shouldn't wait for DAT for stop commands. */
	if (cmd == slot->req->stop)
		mask &= ~SDHCI_DAT_INHIBIT;
	/* Wait for bus no more then 10 ms. */
	timeout = 10;
	while (state & mask) {
		if (timeout == 0) {
			slot_printf(slot, "Controller never released "
			    "inhibit bit(s).\n");
			sdhci_dumpregs(slot);
			cmd->error = MMC_ERR_FAILED;
			slot->req = NULL;
			slot->curcmd = NULL;
			req->done(req);
			return;
		}
		timeout--;
		DELAY(1000);
		state = RD4(slot, SDHCI_PRESENT_STATE);
	}

	/* Prepare command flags. */
	if (!(cmd->flags & MMC_RSP_PRESENT))
		flags = SDHCI_CMD_RESP_NONE;
	else if (cmd->flags & MMC_RSP_136)
		flags = SDHCI_CMD_RESP_LONG;
	else if (cmd->flags & MMC_RSP_BUSY)
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
	else
		flags = SDHCI_CMD_RESP_SHORT;
	if (cmd->flags & MMC_RSP_CRC)
		flags |= SDHCI_CMD_CRC;
	if (cmd->flags & MMC_RSP_OPCODE)
		flags |= SDHCI_CMD_INDEX;
	if (cmd->data)
		flags |= SDHCI_CMD_DATA;
	if (cmd->opcode == MMC_STOP_TRANSMISSION)
		flags |= SDHCI_CMD_TYPE_ABORT;
	/* Prepare data. */
	sdhci_start_data(slot, cmd->data);
	/* 
	 * Interrupt aggregation: To reduce total number of interrupts
	 * group response interrupt with data interrupt when possible.
	 * If there going to be data interrupt, mask response one.
	 */
	if (slot->data_done == 0) {
		WR4(slot, SDHCI_SIGNAL_ENABLE,
		    slot->intmask &= ~SDHCI_INT_RESPONSE);
	}
	/* Set command argument. */
	WR4(slot, SDHCI_ARGUMENT, cmd->arg);
	/* Set data transfer mode. */
	sdhci_set_transfer_mode(slot, cmd->data);
	/* Start command. */
	WR2(slot, SDHCI_COMMAND_FLAGS, (cmd->opcode << 8) | (flags & 0xff));
}

static void
sdhci_finish_command(struct sdhci_slot *slot)
{
	int i;

	slot->cmd_done = 1;
	/* Interrupt aggregation: Restore command interrupt.
	 * Main restore point for the case when command interrupt
	 * happened first. */
	WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask |= SDHCI_INT_RESPONSE);
	/* In case of error - reset host and return. */
	if (slot->curcmd->error) {
		sdhci_reset(slot, SDHCI_RESET_CMD);
		sdhci_reset(slot, SDHCI_RESET_DATA);
		sdhci_start(slot);
		return;
	}
	/* If command has response - fetch it. */
	if (slot->curcmd->flags & MMC_RSP_PRESENT) {
		if (slot->curcmd->flags & MMC_RSP_136) {
			/* CRC is stripped so we need one byte shift. */
			uint8_t extra = 0;
			for (i = 0; i < 4; i++) {
				uint32_t val = RD4(slot, SDHCI_RESPONSE + i * 4);
				if (slot->quirks & SDHCI_QUIRK_DONT_SHIFT_RESPONSE)
					slot->curcmd->resp[3 - i] = val;
				else {
					slot->curcmd->resp[3 - i] = 
					    (val << 8) | extra;
					extra = val >> 24;
				}
			}
		} else
			slot->curcmd->resp[0] = RD4(slot, SDHCI_RESPONSE);
	}
	/* If data ready - finish. */
	if (slot->data_done)
		sdhci_start(slot);
}

static void
sdhci_start_data(struct sdhci_slot *slot, struct mmc_data *data)
{
	uint32_t target_timeout, current_timeout;
	uint8_t div;

	if (data == NULL && (slot->curcmd->flags & MMC_RSP_BUSY) == 0) {
		slot->data_done = 1;
		return;
	}

	slot->data_done = 0;

	/* Calculate and set data timeout.*/
	/* XXX: We should have this from mmc layer, now assume 1 sec. */
	if (slot->quirks & SDHCI_QUIRK_BROKEN_TIMEOUT_VAL) {
		div = 0xE;
	} else {
		target_timeout = 1000000;
		div = 0;
		current_timeout = (1 << 13) * 1000 / slot->timeout_clk;
		while (current_timeout < target_timeout && div < 0xE) {
			++div;
			current_timeout <<= 1;
		}
		/* Compensate for an off-by-one error in the CaFe chip.*/
		if (div < 0xE && 
		    (slot->quirks & SDHCI_QUIRK_INCR_TIMEOUT_CONTROL)) {
			++div;
		}
	}
	WR1(slot, SDHCI_TIMEOUT_CONTROL, div);

	if (data == NULL)
		return;

	/* Use DMA if possible. */
	if ((slot->opt & SDHCI_HAVE_DMA))
		slot->flags |= SDHCI_USE_DMA;
	/* If data is small, broken DMA may return zeroes instead of data, */
	if ((slot->quirks & SDHCI_QUIRK_BROKEN_TIMINGS) &&
	    (data->len <= 512))
		slot->flags &= ~SDHCI_USE_DMA;
	/* Some controllers require even block sizes. */
	if ((slot->quirks & SDHCI_QUIRK_32BIT_DMA_SIZE) &&
	    ((data->len) & 0x3))
		slot->flags &= ~SDHCI_USE_DMA;
	/* Load DMA buffer. */
	if (slot->flags & SDHCI_USE_DMA) {
		if (data->flags & MMC_DATA_READ)
			bus_dmamap_sync(slot->dmatag, slot->dmamap, 
			    BUS_DMASYNC_PREREAD);
		else {
			memcpy(slot->dmamem, data->data,
			    (data->len < DMA_BLOCK_SIZE) ? 
			    data->len : DMA_BLOCK_SIZE);
			bus_dmamap_sync(slot->dmatag, slot->dmamap, 
			    BUS_DMASYNC_PREWRITE);
		}
		WR4(slot, SDHCI_DMA_ADDRESS, slot->paddr);
		/* Interrupt aggregation: Mask border interrupt
		 * for the last page and unmask else. */
		if (data->len == DMA_BLOCK_SIZE)
			slot->intmask &= ~SDHCI_INT_DMA_END;
		else
			slot->intmask |= SDHCI_INT_DMA_END;
		WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask);
	}
	/* Current data offset for both PIO and DMA. */
	slot->offset = 0;
	/* Set block size and request IRQ on 4K border. */
	WR2(slot, SDHCI_BLOCK_SIZE,
	    SDHCI_MAKE_BLKSZ(DMA_BOUNDARY, (data->len < 512)?data->len:512));
	/* Set block count. */
	WR2(slot, SDHCI_BLOCK_COUNT, (data->len + 511) / 512);
}

void
sdhci_finish_data(struct sdhci_slot *slot)
{
	struct mmc_data *data = slot->curcmd->data;

	slot->data_done = 1;
	/* Interrupt aggregation: Restore command interrupt.
	 * Auxiliary restore point for the case when data interrupt
	 * happened first. */
	if (!slot->cmd_done) {
		WR4(slot, SDHCI_SIGNAL_ENABLE,
		    slot->intmask |= SDHCI_INT_RESPONSE);
	}
	/* Unload rest of data from DMA buffer. */
	if (slot->flags & SDHCI_USE_DMA) {
		if (data->flags & MMC_DATA_READ) {
			size_t left = data->len - slot->offset;
			bus_dmamap_sync(slot->dmatag, slot->dmamap, 
			    BUS_DMASYNC_POSTREAD);
			memcpy((u_char*)data->data + slot->offset, slot->dmamem,
			    (left < DMA_BLOCK_SIZE)?left:DMA_BLOCK_SIZE);
		} else
			bus_dmamap_sync(slot->dmatag, slot->dmamap, 
			    BUS_DMASYNC_POSTWRITE);
	}
	/* If there was error - reset the host. */
	if (slot->curcmd->error) {
		sdhci_reset(slot, SDHCI_RESET_CMD);
		sdhci_reset(slot, SDHCI_RESET_DATA);
		sdhci_start(slot);
		return;
	}
	/* If we already have command response - finish. */
	if (slot->cmd_done)
		sdhci_start(slot);
}

static void
sdhci_start(struct sdhci_slot *slot)
{
	struct mmc_request *req;

	req = slot->req;
	if (req == NULL)
		return;

	if (!(slot->flags & CMD_STARTED)) {
		slot->flags |= CMD_STARTED;
		sdhci_start_command(slot, req->cmd);
		return;
	}
/* 	We don't need this until using Auto-CMD12 feature
	if (!(slot->flags & STOP_STARTED) && req->stop) {
		slot->flags |= STOP_STARTED;
		sdhci_start_command(slot, req->stop);
		return;
	}
*/
	if (sdhci_debug > 1)
		slot_printf(slot, "result: %d\n", req->cmd->error);
	if (!req->cmd->error &&
	    (slot->quirks & SDHCI_QUIRK_RESET_AFTER_REQUEST)) {
		sdhci_reset(slot, SDHCI_RESET_CMD);
		sdhci_reset(slot, SDHCI_RESET_DATA);
	}

	/* We must be done -- bad idea to do this while locked? */
	slot->req = NULL;
	slot->curcmd = NULL;
	req->done(req);
}

int
sdhci_generic_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);

	SDHCI_LOCK(slot);
	if (slot->req != NULL) {
		SDHCI_UNLOCK(slot);
		return (EBUSY);
	}
	if (sdhci_debug > 1) {
		slot_printf(slot, "CMD%u arg %#x flags %#x dlen %u dflags %#x\n",
    		    req->cmd->opcode, req->cmd->arg, req->cmd->flags,
    		    (req->cmd->data)?(u_int)req->cmd->data->len:0,
		    (req->cmd->data)?req->cmd->data->flags:0);
	}
	slot->req = req;
	slot->flags = 0;
	sdhci_start(slot);
	SDHCI_UNLOCK(slot);
	if (dumping) {
		while (slot->req != NULL) {
			sdhci_generic_intr(slot);
			DELAY(10);
		}
	}
	return (0);
}

int
sdhci_generic_get_ro(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);
	uint32_t val;

	SDHCI_LOCK(slot);
	val = RD4(slot, SDHCI_PRESENT_STATE);
	SDHCI_UNLOCK(slot);
	return (!(val & SDHCI_WRITE_PROTECT));
}

int
sdhci_generic_acquire_host(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);
	int err = 0;

	SDHCI_LOCK(slot);
	while (slot->bus_busy)
		msleep(slot, &slot->mtx, 0, "sdhciah", 0);
	slot->bus_busy++;
	/* Activate led. */
	WR1(slot, SDHCI_HOST_CONTROL, slot->hostctrl |= SDHCI_CTRL_LED);
	SDHCI_UNLOCK(slot);
	return (err);
}

int
sdhci_generic_release_host(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);

	SDHCI_LOCK(slot);
	/* Deactivate led. */
	WR1(slot, SDHCI_HOST_CONTROL, slot->hostctrl &= ~SDHCI_CTRL_LED);
	slot->bus_busy--;
	SDHCI_UNLOCK(slot);
	wakeup(slot);
	return (0);
}

static void
sdhci_cmd_irq(struct sdhci_slot *slot, uint32_t intmask)
{

	if (!slot->curcmd) {
		slot_printf(slot, "Got command interrupt 0x%08x, but "
		    "there is no active command.\n", intmask);
		sdhci_dumpregs(slot);
		return;
	}
	if (intmask & SDHCI_INT_TIMEOUT)
		slot->curcmd->error = MMC_ERR_TIMEOUT;
	else if (intmask & SDHCI_INT_CRC)
		slot->curcmd->error = MMC_ERR_BADCRC;
	else if (intmask & (SDHCI_INT_END_BIT | SDHCI_INT_INDEX))
		slot->curcmd->error = MMC_ERR_FIFO;

	sdhci_finish_command(slot);
}

static void
sdhci_data_irq(struct sdhci_slot *slot, uint32_t intmask)
{

	if (!slot->curcmd) {
		slot_printf(slot, "Got data interrupt 0x%08x, but "
		    "there is no active command.\n", intmask);
		sdhci_dumpregs(slot);
		return;
	}
	if (slot->curcmd->data == NULL &&
	    (slot->curcmd->flags & MMC_RSP_BUSY) == 0) {
		slot_printf(slot, "Got data interrupt 0x%08x, but "
		    "there is no active data operation.\n",
		    intmask);
		sdhci_dumpregs(slot);
		return;
	}
	if (intmask & SDHCI_INT_DATA_TIMEOUT)
		slot->curcmd->error = MMC_ERR_TIMEOUT;
	else if (intmask & (SDHCI_INT_DATA_CRC | SDHCI_INT_DATA_END_BIT))
		slot->curcmd->error = MMC_ERR_BADCRC;
	if (slot->curcmd->data == NULL &&
	    (intmask & (SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL |
	    SDHCI_INT_DMA_END))) {
		slot_printf(slot, "Got data interrupt 0x%08x, but "
		    "there is busy-only command.\n", intmask);
		sdhci_dumpregs(slot);
		slot->curcmd->error = MMC_ERR_INVALID;
	}
	if (slot->curcmd->error) {
		/* No need to continue after any error. */
		if (slot->flags & PLATFORM_DATA_STARTED) {
			slot->flags &= ~PLATFORM_DATA_STARTED;
			SDHCI_PLATFORM_FINISH_TRANSFER(slot->bus, slot);
		} else
			sdhci_finish_data(slot);
		return;
	}

	/* Handle PIO interrupt. */
	if (intmask & (SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL)) {
		if ((slot->opt & SDHCI_PLATFORM_TRANSFER) && 
		    SDHCI_PLATFORM_WILL_HANDLE(slot->bus, slot)) {
			SDHCI_PLATFORM_START_TRANSFER(slot->bus, slot, &intmask);
			slot->flags |= PLATFORM_DATA_STARTED;
		} else
			sdhci_transfer_pio(slot);
	}
	/* Handle DMA border. */
	if (intmask & SDHCI_INT_DMA_END) {
		struct mmc_data *data = slot->curcmd->data;
		size_t left;

		/* Unload DMA buffer... */
		left = data->len - slot->offset;
		if (data->flags & MMC_DATA_READ) {
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_POSTREAD);
			memcpy((u_char*)data->data + slot->offset, slot->dmamem,
			    (left < DMA_BLOCK_SIZE)?left:DMA_BLOCK_SIZE);
		} else {
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_POSTWRITE);
		}
		/* ... and reload it again. */
		slot->offset += DMA_BLOCK_SIZE;
		left = data->len - slot->offset;
		if (data->flags & MMC_DATA_READ) {
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_PREREAD);
		} else {
			memcpy(slot->dmamem, (u_char*)data->data + slot->offset,
			    (left < DMA_BLOCK_SIZE)?left:DMA_BLOCK_SIZE);
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_PREWRITE);
		}
		/* Interrupt aggregation: Mask border interrupt
		 * for the last page. */
		if (left == DMA_BLOCK_SIZE) {
			slot->intmask &= ~SDHCI_INT_DMA_END;
			WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask);
		}
		/* Restart DMA. */
		WR4(slot, SDHCI_DMA_ADDRESS, slot->paddr);
	}
	/* We have got all data. */
	if (intmask & SDHCI_INT_DATA_END) {
		if (slot->flags & PLATFORM_DATA_STARTED) {
			slot->flags &= ~PLATFORM_DATA_STARTED;
			SDHCI_PLATFORM_FINISH_TRANSFER(slot->bus, slot);
		} else
			sdhci_finish_data(slot);
	}
}

static void
sdhci_acmd_irq(struct sdhci_slot *slot)
{
	uint16_t err;
	
	err = RD4(slot, SDHCI_ACMD12_ERR);
	if (!slot->curcmd) {
		slot_printf(slot, "Got AutoCMD12 error 0x%04x, but "
		    "there is no active command.\n", err);
		sdhci_dumpregs(slot);
		return;
	}
	slot_printf(slot, "Got AutoCMD12 error 0x%04x\n", err);
	sdhci_reset(slot, SDHCI_RESET_CMD);
}

void
sdhci_generic_intr(struct sdhci_slot *slot)
{
	uint32_t intmask;
	
	SDHCI_LOCK(slot);
	/* Read slot interrupt status. */
	intmask = RD4(slot, SDHCI_INT_STATUS);
	if (intmask == 0 || intmask == 0xffffffff) {
		SDHCI_UNLOCK(slot);
		return;
	}
	if (sdhci_debug > 2)
		slot_printf(slot, "Interrupt %#x\n", intmask);

	/* Handle card presence interrupts. */
	if (intmask & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
		WR4(slot, SDHCI_INT_STATUS, intmask & 
		    (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE));

		if (intmask & SDHCI_INT_CARD_REMOVE) {
			if (bootverbose || sdhci_debug)
				slot_printf(slot, "Card removed\n");
			callout_stop(&slot->card_callout);
			taskqueue_enqueue(taskqueue_swi_giant,
			    &slot->card_task);
		}
		if (intmask & SDHCI_INT_CARD_INSERT) {
			if (bootverbose || sdhci_debug)
				slot_printf(slot, "Card inserted\n");
			callout_reset(&slot->card_callout, hz / 2,
			    sdhci_card_delay, slot);
		}
		intmask &= ~(SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE);
	}
	/* Handle command interrupts. */
	if (intmask & SDHCI_INT_CMD_MASK) {
		WR4(slot, SDHCI_INT_STATUS, intmask & SDHCI_INT_CMD_MASK);
		sdhci_cmd_irq(slot, intmask & SDHCI_INT_CMD_MASK);
	}
	/* Handle data interrupts. */
	if (intmask & SDHCI_INT_DATA_MASK) {
		WR4(slot, SDHCI_INT_STATUS, intmask & SDHCI_INT_DATA_MASK);
		sdhci_data_irq(slot, intmask & SDHCI_INT_DATA_MASK);
	}
	/* Handle AutoCMD12 error interrupt. */
	if (intmask & SDHCI_INT_ACMD12ERR) {
		WR4(slot, SDHCI_INT_STATUS, SDHCI_INT_ACMD12ERR);
		sdhci_acmd_irq(slot);
	}
	intmask &= ~(SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK);
	intmask &= ~SDHCI_INT_ACMD12ERR;
	intmask &= ~SDHCI_INT_ERROR;
	/* Handle bus power interrupt. */
	if (intmask & SDHCI_INT_BUS_POWER) {
		WR4(slot, SDHCI_INT_STATUS, SDHCI_INT_BUS_POWER);
		slot_printf(slot,
		    "Card is consuming too much power!\n");
		intmask &= ~SDHCI_INT_BUS_POWER;
	}
	/* The rest is unknown. */
	if (intmask) {
		WR4(slot, SDHCI_INT_STATUS, intmask);
		slot_printf(slot, "Unexpected interrupt 0x%08x.\n",
		    intmask);
		sdhci_dumpregs(slot);
	}
	
	SDHCI_UNLOCK(slot);
}

int
sdhci_generic_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct sdhci_slot *slot = device_get_ivars(child);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*result = slot->host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*result = slot->host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*result = slot->host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*result = slot->host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*result = slot->host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*result = slot->host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*result = slot->host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*result = slot->host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*result = slot->host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*result = slot->host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*result = slot->host.ios.vdd;
		break;
	case MMCBR_IVAR_CAPS:
		*result = slot->host.caps;
		break;
	case MMCBR_IVAR_TIMING:
		*result = slot->host.ios.timing;
		break;
	case MMCBR_IVAR_MAX_DATA:
		*result = 65535;
		break;
	}
	return (0);
}

int
sdhci_generic_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct sdhci_slot *slot = device_get_ivars(child);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		slot->host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		slot->host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		slot->host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		if (value > 0) {
			uint32_t max_clock;
			uint32_t clock;
			int i;

			max_clock = slot->max_clk;
			clock = max_clock;

			if (slot->version < SDHCI_SPEC_300) {
				for (i = 0; i < SDHCI_200_MAX_DIVIDER;
				    i <<= 1) {
					if (clock <= value)
						break;
					clock >>= 1;
				}
			}
			else {
				for (i = 0; i < SDHCI_300_MAX_DIVIDER;
				    i += 2) {
					if (clock <= value)
						break;
					clock = max_clock / (i + 2);
				}
			}

			slot->host.ios.clock = clock;
		} else
			slot->host.ios.clock = 0;
		break;
	case MMCBR_IVAR_MODE:
		slot->host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		slot->host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		slot->host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		slot->host.ios.vdd = value;
		break;
	case MMCBR_IVAR_TIMING:
		slot->host.ios.timing = value;
		break;
	case MMCBR_IVAR_CAPS:
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	case MMCBR_IVAR_MAX_DATA:
		return (EINVAL);
	}
	return (0);
}

MODULE_VERSION(sdhci, 1);
