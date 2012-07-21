/*-
 * Copyright (c) 2006 Bernd Walter.  All rights reserved.
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <geom/geom_disk.h>

#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcvar.h>

#include "mmcbus_if.h"

#if __FreeBSD_version < 800002
#define	kproc_create	kthread_create
#define	kproc_exit	kthread_exit
#endif

struct mmcsd_softc {
	device_t dev;
	struct mtx sc_mtx;
	struct disk *disk;
	struct proc *p;
	struct bio_queue_head bio_queue;
	daddr_t eblock, eend;	/* Range remaining after the last erase. */
	int running;
	int suspend;
};

/* bus entry points */
static int mmcsd_attach(device_t dev);
static int mmcsd_detach(device_t dev);
static int mmcsd_probe(device_t dev);

/* disk routines */
static int mmcsd_close(struct disk *dp);
static int mmcsd_dump(void *arg, void *virtual, vm_offset_t physical,
	off_t offset, size_t length);
static int mmcsd_open(struct disk *dp);
static void mmcsd_strategy(struct bio *bp);
static void mmcsd_task(void *arg);

static int mmcsd_bus_bit_width(device_t dev);
static daddr_t mmcsd_delete(struct mmcsd_softc *sc, struct bio *bp);
static daddr_t mmcsd_rw(struct mmcsd_softc *sc, struct bio *bp);

#define MMCSD_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MMCSD_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define MMCSD_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "mmcsd", MTX_DEF)
#define MMCSD_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define MMCSD_ASSERT_LOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define MMCSD_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static int
mmcsd_probe(device_t dev)
{

	device_quiet(dev);
	device_set_desc(dev, "MMC/SD Memory Card");
	return (0);
}

static int
mmcsd_attach(device_t dev)
{
	struct mmcsd_softc *sc;
	struct disk *d;
	intmax_t mb;
	uint32_t speed;
	uint32_t maxblocks;
	char unit;

	sc = device_get_softc(dev);
	sc->dev = dev;
	MMCSD_LOCK_INIT(sc);

	d = sc->disk = disk_alloc();
	d->d_open = mmcsd_open;
	d->d_close = mmcsd_close;
	d->d_strategy = mmcsd_strategy;
	d->d_dump = mmcsd_dump;
	d->d_name = "mmcsd";
	d->d_drv1 = sc;
	d->d_maxsize = 4*1024*1024;	/* Maximum defined SD card AU size. */
	d->d_sectorsize = mmc_get_sector_size(dev);
	d->d_mediasize = (off_t)mmc_get_media_size(dev) * d->d_sectorsize;
	d->d_stripeoffset = 0;
	d->d_stripesize = mmc_get_erase_sector(dev) * d->d_sectorsize;
	d->d_unit = device_get_unit(dev);
	d->d_flags = DISKFLAG_CANDELETE;
	/*
	 * Display in most natural units.  There's no cards < 1MB.
	 * The SD standard goes to 2GiB, but the data format supports
	 * up to 4GiB and some card makers push it up to this limit.
	 * The SDHC standard only goes to 32GiB (the data format in
	 * SDHC is good to 2TiB however, which isn't too ugly at
	 * 2048GiBm, so we note it in passing here and don't add the
	 * code to print TiB).
	 */
	mb = d->d_mediasize >> 20;	/* 1MiB == 1 << 20 */
	unit = 'M';
	if (mb >= 10240) {		/* 1GiB = 1024 MiB */
		unit = 'G';
		mb /= 1024;
	}
	/*
	 * Report the clock speed of the underlying hardware, which might be
	 * different than what the card reports due to hardware limitations.
	 * Report how many blocks the hardware transfers at once, but clip the
	 * number to MAXPHYS since the system won't initiate larger transfers.
	 */
	speed = mmcbr_get_clock(device_get_parent(dev));
	maxblocks = mmc_get_max_data(dev);
	if (maxblocks > MAXPHYS)
		maxblocks = MAXPHYS;
	device_printf(dev, "%ju%cB <%s>%s at %s %d.%01dMHz/%dbit/%d-block\n",
	    mb, unit, mmc_get_card_id_string(dev),
	    mmc_get_read_only(dev) ? " (read-only)" : "",
	    device_get_nameunit(device_get_parent(dev)),
	    speed / 1000000, (speed / 100000) % 10,
	    mmcsd_bus_bit_width(dev), maxblocks);
	disk_create(d, DISK_VERSION);
	bioq_init(&sc->bio_queue);

	sc->running = 1;
	sc->suspend = 0;
	sc->eblock = sc->eend = 0;
	kproc_create(&mmcsd_task, sc, &sc->p, 0, 0, "task: mmc/sd card");

	return (0);
}

static int
mmcsd_detach(device_t dev)
{
	struct mmcsd_softc *sc = device_get_softc(dev);

	MMCSD_LOCK(sc);
	sc->suspend = 0;
	if (sc->running > 0) {
		/* kill thread */
		sc->running = 0;
		wakeup(sc);
		/* wait for thread to finish. */
		while (sc->running != -1)
			msleep(sc, &sc->sc_mtx, 0, "detach", 0);
	}
	MMCSD_UNLOCK(sc);

	/* Flush the request queue. */
	bioq_flush(&sc->bio_queue, NULL, ENXIO);
	/* kill disk */
	disk_destroy(sc->disk);

	MMCSD_LOCK_DESTROY(sc);

	return (0);
}

static int
mmcsd_suspend(device_t dev)
{
	struct mmcsd_softc *sc = device_get_softc(dev);

	MMCSD_LOCK(sc);
	sc->suspend = 1;
	if (sc->running > 0) {
		/* kill thread */
		sc->running = 0;
		wakeup(sc);
		/* wait for thread to finish. */
		while (sc->running != -1)
			msleep(sc, &sc->sc_mtx, 0, "detach", 0);
	}
	MMCSD_UNLOCK(sc);
	return (0);
}

static int
mmcsd_resume(device_t dev)
{
	struct mmcsd_softc *sc = device_get_softc(dev);

	MMCSD_LOCK(sc);
	sc->suspend = 0;
	if (sc->running <= 0) {
		sc->running = 1;
		MMCSD_UNLOCK(sc);
		kproc_create(&mmcsd_task, sc, &sc->p, 0, 0, "task: mmc/sd card");
	} else
		MMCSD_UNLOCK(sc);
	return (0);
}

static int
mmcsd_open(struct disk *dp)
{

	return (0);
}

static int
mmcsd_close(struct disk *dp)
{

	return (0);
}

static void
mmcsd_strategy(struct bio *bp)
{
	struct mmcsd_softc *sc;

	sc = (struct mmcsd_softc *)bp->bio_disk->d_drv1;
	MMCSD_LOCK(sc);
	if (sc->running > 0 || sc->suspend > 0) {
		bioq_disksort(&sc->bio_queue, bp);
		MMCSD_UNLOCK(sc);
		wakeup(sc);
	} else {
		MMCSD_UNLOCK(sc);
		biofinish(bp, NULL, ENXIO);
	}
}

static daddr_t
mmcsd_rw(struct mmcsd_softc *sc, struct bio *bp)
{
	daddr_t block, end;
	struct mmc_command cmd;
	struct mmc_command stop;
	struct mmc_request req;
	struct mmc_data data;
	device_t dev = sc->dev;
	int sz = sc->disk->d_sectorsize;

	block = bp->bio_pblkno;
	end = bp->bio_pblkno + (bp->bio_bcount / sz);
	while (block < end) {
		char *vaddr = bp->bio_data +
		    (block - bp->bio_pblkno) * sz;
		int numblocks = min(end - block, mmc_get_max_data(dev));
		memset(&req, 0, sizeof(req));
    		memset(&cmd, 0, sizeof(cmd));
		memset(&stop, 0, sizeof(stop));
		req.cmd = &cmd;
		cmd.data = &data;
		if (bp->bio_cmd == BIO_READ) {
			if (numblocks > 1)
				cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
			else
				cmd.opcode = MMC_READ_SINGLE_BLOCK;
		} else {
			if (numblocks > 1)
				cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
			else
				cmd.opcode = MMC_WRITE_BLOCK;
		}
		cmd.arg = block;
		if (!mmc_get_high_cap(dev))
			cmd.arg <<= 9;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		data.data = vaddr;
		data.mrq = &req;
		if (bp->bio_cmd == BIO_READ)
			data.flags = MMC_DATA_READ;
		else
			data.flags = MMC_DATA_WRITE;
		data.len = numblocks * sz;
		if (numblocks > 1) {
			data.flags |= MMC_DATA_MULTI;
			stop.opcode = MMC_STOP_TRANSMISSION;
			stop.arg = 0;
			stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
			req.stop = &stop;
		}
//		printf("Len %d  %lld-%lld flags %#x sz %d\n",
//		    (int)data.len, (long long)block, (long long)end, data.flags, sz);
		MMCBUS_WAIT_FOR_REQUEST(device_get_parent(dev), dev,
		    &req);
		if (req.cmd->error != MMC_ERR_NONE)
			break;
		block += numblocks;
	}
	return (block);
}

static daddr_t
mmcsd_delete(struct mmcsd_softc *sc, struct bio *bp)
{
	daddr_t block, end, start, stop;
	struct mmc_command cmd;
	struct mmc_request req;
	device_t dev = sc->dev;
	int sz = sc->disk->d_sectorsize;
	int erase_sector;

	block = bp->bio_pblkno;
	end = bp->bio_pblkno + (bp->bio_bcount / sz);
	/* Coalesce with part remaining from previous request. */
	if (block > sc->eblock && block <= sc->eend)
		block = sc->eblock;
	if (end >= sc->eblock && end < sc->eend)
		end = sc->eend;
	/* Safe round to the erase sector boundaries. */
	erase_sector = mmc_get_erase_sector(dev);
	start = block + erase_sector - 1;	 /* Round up. */
	start -= start % erase_sector;
	stop = end;				/* Round down. */
	stop -= end % erase_sector;	 
	/* We can't erase area smaller then sector, store it for later. */
	if (start >= stop) {
		sc->eblock = block;
		sc->eend = end;
		return (end);
	}

	/* Set erase start position. */
	memset(&req, 0, sizeof(req));
	memset(&cmd, 0, sizeof(cmd));
	req.cmd = &cmd;
	if (mmc_get_card_type(dev) == mode_sd)
		cmd.opcode = SD_ERASE_WR_BLK_START;
	else
		cmd.opcode = MMC_ERASE_GROUP_START;
	cmd.arg = start;
	if (!mmc_get_high_cap(dev))
		cmd.arg <<= 9;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	MMCBUS_WAIT_FOR_REQUEST(device_get_parent(dev), dev,
	    &req);
	if (req.cmd->error != MMC_ERR_NONE) {
	    printf("erase err1: %d\n", req.cmd->error);
	    return (block);
	}
	/* Set erase stop position. */
	memset(&req, 0, sizeof(req));
	memset(&cmd, 0, sizeof(cmd));
	req.cmd = &cmd;
	if (mmc_get_card_type(dev) == mode_sd)
		cmd.opcode = SD_ERASE_WR_BLK_END;
	else
		cmd.opcode = MMC_ERASE_GROUP_END;
	cmd.arg = stop;
	if (!mmc_get_high_cap(dev))
		cmd.arg <<= 9;
	cmd.arg--;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	MMCBUS_WAIT_FOR_REQUEST(device_get_parent(dev), dev,
	    &req);
	if (req.cmd->error != MMC_ERR_NONE) {
	    printf("erase err2: %d\n", req.cmd->error);
	    return (block);
	}
	/* Erase range. */
	memset(&req, 0, sizeof(req));
	memset(&cmd, 0, sizeof(cmd));
	req.cmd = &cmd;
	cmd.opcode = MMC_ERASE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	MMCBUS_WAIT_FOR_REQUEST(device_get_parent(dev), dev,
	    &req);
	if (req.cmd->error != MMC_ERR_NONE) {
	    printf("erase err3 %d\n", req.cmd->error);
	    return (block);
	}
	/* Store one of remaining parts for the next call. */
	if (bp->bio_pblkno >= sc->eblock || block == start) {
		sc->eblock = stop;	/* Predict next forward. */
		sc->eend = end;
	} else {
		sc->eblock = block;	/* Predict next backward. */
		sc->eend = start;
	}
	return (end);
}

static int
mmcsd_dump(void *arg, void *virtual, vm_offset_t physical,
	off_t offset, size_t length)
{
	struct disk *disk = arg;
	struct mmcsd_softc *sc = (struct mmcsd_softc *)disk->d_drv1;
	device_t dev = sc->dev;
	struct bio bp;
	daddr_t block, end;

	/* length zero is special and really means flush buffers to media */
	if (!length)
		return (0);

	bzero(&bp, sizeof(struct bio));
	bp.bio_disk = disk;
	bp.bio_pblkno = offset / disk->d_sectorsize;
	bp.bio_bcount = length;
	bp.bio_data = virtual;
	bp.bio_cmd = BIO_WRITE;
	end = bp.bio_pblkno + bp.bio_bcount / sc->disk->d_sectorsize;
	MMCBUS_ACQUIRE_BUS(device_get_parent(dev), dev);
	block = mmcsd_rw(sc, &bp);
	MMCBUS_RELEASE_BUS(device_get_parent(dev), dev);
	return ((end < block) ? EIO : 0);
}

static void
mmcsd_task(void *arg)
{
	struct mmcsd_softc *sc = (struct mmcsd_softc*)arg;
	struct bio *bp;
	int sz;
	daddr_t block, end;
	device_t dev;

	dev = sc->dev;
	while (1) {
		MMCSD_LOCK(sc);
		do {
			if (sc->running == 0)
				goto out;
			bp = bioq_takefirst(&sc->bio_queue);
			if (bp == NULL)
				msleep(sc, &sc->sc_mtx, PRIBIO, "jobqueue", 0);
		} while (bp == NULL);
		MMCSD_UNLOCK(sc);
		if (bp->bio_cmd != BIO_READ && mmc_get_read_only(dev)) {
			bp->bio_error = EROFS;
			bp->bio_resid = bp->bio_bcount;
			bp->bio_flags |= BIO_ERROR;
			biodone(bp);
			continue;
		}
		MMCBUS_ACQUIRE_BUS(device_get_parent(dev), dev);
		sz = sc->disk->d_sectorsize;
		block = bp->bio_pblkno;
		end = bp->bio_pblkno + (bp->bio_bcount / sz);
		if (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE) {
			/* Access to the remaining erase block obsoletes it. */
			if (block < sc->eend && end > sc->eblock)
				sc->eblock = sc->eend = 0;
			block = mmcsd_rw(sc, bp);
		} else if (bp->bio_cmd == BIO_DELETE) {
			block = mmcsd_delete(sc, bp);
		}
		MMCBUS_RELEASE_BUS(device_get_parent(dev), dev);
		if (block < end) {
			bp->bio_error = EIO;
			bp->bio_resid = (end - block) * sz;
			bp->bio_flags |= BIO_ERROR;
		}
		biodone(bp);
	}
out:
	/* tell parent we're done */
	sc->running = -1;
	MMCSD_UNLOCK(sc);
	wakeup(sc);

	kproc_exit(0);
}

static int
mmcsd_bus_bit_width(device_t dev)
{

	if (mmc_get_bus_width(dev) == bus_width_1)
		return (1);
	if (mmc_get_bus_width(dev) == bus_width_4)
		return (4);
	return (8);
}

static device_method_t mmcsd_methods[] = {
	DEVMETHOD(device_probe, mmcsd_probe),
	DEVMETHOD(device_attach, mmcsd_attach),
	DEVMETHOD(device_detach, mmcsd_detach),
	DEVMETHOD(device_suspend, mmcsd_suspend),
	DEVMETHOD(device_resume, mmcsd_resume),
	DEVMETHOD_END
};

static driver_t mmcsd_driver = {
	"mmcsd",
	mmcsd_methods,
	sizeof(struct mmcsd_softc),
};
static devclass_t mmcsd_devclass;

DRIVER_MODULE(mmcsd, mmc, mmcsd_driver, mmcsd_devclass, NULL, NULL);
