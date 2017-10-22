/*-
 * Copyright (c) 2003, 2008 Silicon Graphics International Corp.
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend_ramdisk.c#3 $
 */
/*
 * CAM Target Layer backend for a "fake" ramdisk.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/module.h>

#include <cam/scsi/scsi_all.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_error.h>

typedef enum {
	CTL_BE_RAMDISK_LUN_UNCONFIGURED	= 0x01,
	CTL_BE_RAMDISK_LUN_CONFIG_ERR	= 0x02,
	CTL_BE_RAMDISK_LUN_WAITING	= 0x04
} ctl_be_ramdisk_lun_flags;

struct ctl_be_ramdisk_lun {
	uint64_t size_bytes;
	uint64_t size_blocks;
	struct ctl_be_ramdisk_softc *softc;
	ctl_be_ramdisk_lun_flags flags;
	STAILQ_ENTRY(ctl_be_ramdisk_lun) links;
	struct ctl_be_lun ctl_be_lun;
};

struct ctl_be_ramdisk_softc {
	struct mtx lock;
	int rd_size;
#ifdef CTL_RAMDISK_PAGES
	uint8_t **ramdisk_pages;
	int num_pages;
#else
	uint8_t *ramdisk_buffer;
#endif
	int num_luns;
	STAILQ_HEAD(, ctl_be_ramdisk_lun) lun_list;
};

static struct ctl_be_ramdisk_softc rd_softc;

int ctl_backend_ramdisk_init(void);
void ctl_backend_ramdisk_shutdown(void);
static int ctl_backend_ramdisk_move_done(union ctl_io *io);
static int ctl_backend_ramdisk_submit(union ctl_io *io);
static int ctl_backend_ramdisk_ioctl(struct cdev *dev, u_long cmd,
				     caddr_t addr, int flag, struct thread *td);
static int ctl_backend_ramdisk_rm(struct ctl_be_ramdisk_softc *softc,
				  struct ctl_lun_req *req);
static int ctl_backend_ramdisk_create(struct ctl_be_ramdisk_softc *softc,
				      struct ctl_lun_req *req, int do_wait);
static int ctl_backend_ramdisk_modify(struct ctl_be_ramdisk_softc *softc,
				  struct ctl_lun_req *req);
static void ctl_backend_ramdisk_lun_shutdown(void *be_lun);
static void ctl_backend_ramdisk_lun_config_status(void *be_lun,
						  ctl_lun_config_status status);
static int ctl_backend_ramdisk_config_write(union ctl_io *io);
static int ctl_backend_ramdisk_config_read(union ctl_io *io);

static struct ctl_backend_driver ctl_be_ramdisk_driver = 
{
	.name = "ramdisk",
	.flags = CTL_BE_FLAG_HAS_CONFIG,
	.init = ctl_backend_ramdisk_init,
	.data_submit = ctl_backend_ramdisk_submit,
	.data_move_done = ctl_backend_ramdisk_move_done,
	.config_read = ctl_backend_ramdisk_config_read,
	.config_write = ctl_backend_ramdisk_config_write,
	.ioctl = ctl_backend_ramdisk_ioctl
};

MALLOC_DEFINE(M_RAMDISK, "ramdisk", "Memory used for CTL RAMdisk");
CTL_BACKEND_DECLARE(cbr, ctl_be_ramdisk_driver);

int
ctl_backend_ramdisk_init(void)
{
	struct ctl_be_ramdisk_softc *softc;
#ifdef CTL_RAMDISK_PAGES
	int i, j;
#endif


	softc = &rd_softc;

	memset(softc, 0, sizeof(*softc));

	mtx_init(&softc->lock, "ramdisk", NULL, MTX_DEF);

	STAILQ_INIT(&softc->lun_list);
	softc->rd_size = 4 * 1024 * 1024;
#ifdef CTL_RAMDISK_PAGES
	softc->num_pages = softc->rd_size / PAGE_SIZE;
	softc->ramdisk_pages = (uint8_t **)malloc(sizeof(uint8_t *) *
						  softc->num_pages, M_RAMDISK,
						  M_WAITOK);
	for (i = 0; i < softc->num_pages; i++) {
		softc->ramdisk_pages[i] = malloc(PAGE_SIZE, M_RAMDISK,M_WAITOK);
		if (softc->ramdisk_pages[i] == NULL) {
			for (j = 0; j < i; j++) {
				free(softc->ramdisk_pages[j], M_RAMDISK);
			}
			free(softc->ramdisk_pages, M_RAMDISK);
			panic("RAMDisk initialization failed\n");
			return (1); /* NOTREACHED */
		}
	}
#else
	softc->ramdisk_buffer = (uint8_t *)malloc(softc->rd_size, M_RAMDISK,
						  M_WAITOK);
#endif

	return (0);
}

void
ctl_backend_ramdisk_shutdown(void)
{
	struct ctl_be_ramdisk_softc *softc;
	struct ctl_be_ramdisk_lun *lun, *next_lun;
#ifdef CTL_RAMDISK_PAGES
	int i;
#endif

	softc = &rd_softc;

	mtx_lock(&softc->lock);
	for (lun = STAILQ_FIRST(&softc->lun_list); lun != NULL; lun = next_lun){
		/*
		 * Grab the next LUN.  The current LUN may get removed by
		 * ctl_invalidate_lun(), which will call our LUN shutdown
		 * routine, if there is no outstanding I/O for this LUN.
		 */
		next_lun = STAILQ_NEXT(lun, links);

		/*
		 * Drop our lock here.  Since ctl_invalidate_lun() can call
		 * back into us, this could potentially lead to a recursive
		 * lock of the same mutex, which would cause a hang.
		 */
		mtx_unlock(&softc->lock);
		ctl_disable_lun(&lun->ctl_be_lun);
		ctl_invalidate_lun(&lun->ctl_be_lun);
		mtx_lock(&softc->lock);
	}
	mtx_unlock(&softc->lock);
	
#ifdef CTL_RAMDISK_PAGES
	for (i = 0; i < softc->num_pages; i++)
		free(softc->ramdisk_pages[i], M_RAMDISK);

	free(softc->ramdisk_pages, M_RAMDISK);
#else
	free(softc->ramdisk_buffer, M_RAMDISK);
#endif

	if (ctl_backend_deregister(&ctl_be_ramdisk_driver) != 0) {
		printf("ctl_backend_ramdisk_shutdown: "
		       "ctl_backend_deregister() failed!\n");
	}
}

static int
ctl_backend_ramdisk_move_done(union ctl_io *io)
{
#ifdef CTL_TIME_IO
	struct bintime cur_bt;
#endif

	CTL_DEBUG_PRINT(("ctl_backend_ramdisk_move_done\n"));
	if ((io->io_hdr.port_status == 0)
	 && ((io->io_hdr.flags & CTL_FLAG_ABORT) == 0)
	 && ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_STATUS_NONE))
		io->io_hdr.status = CTL_SUCCESS;
	else if ((io->io_hdr.port_status != 0)
	      && ((io->io_hdr.flags & CTL_FLAG_ABORT) == 0)
	      && ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_STATUS_NONE)){
		/*
		 * For hardware error sense keys, the sense key
		 * specific value is defined to be a retry count,
		 * but we use it to pass back an internal FETD
		 * error code.  XXX KDM  Hopefully the FETD is only
		 * using 16 bits for an error code, since that's
		 * all the space we have in the sks field.
		 */
		ctl_set_internal_failure(&io->scsiio,
					 /*sks_valid*/ 1,
					 /*retry_count*/
					 io->io_hdr.port_status);
	}
#ifdef CTL_TIME_IO
	getbintime(&cur_bt);
	bintime_sub(&cur_bt, &io->io_hdr.dma_start_bt);
	bintime_add(&io->io_hdr.dma_bt, &cur_bt);
	io->io_hdr.num_dmas++;
#endif

	if (io->scsiio.kern_sg_entries > 0)
		free(io->scsiio.kern_data_ptr, M_RAMDISK);
	ctl_done(io);
	return(0);
}

static int
ctl_backend_ramdisk_submit(union ctl_io *io)
{
	struct ctl_lba_len lbalen;
#ifdef CTL_RAMDISK_PAGES
	struct ctl_sg_entry *sg_entries;
	int len_filled;
	int i;
#endif
	int num_sg_entries, len;
	struct ctl_be_ramdisk_softc *softc;
	struct ctl_be_lun *ctl_be_lun;
	struct ctl_be_ramdisk_lun *be_lun;

	softc = &rd_softc;
	
	ctl_be_lun = (struct ctl_be_lun *)io->io_hdr.ctl_private[
		CTL_PRIV_BACKEND_LUN].ptr;
	be_lun = (struct ctl_be_ramdisk_lun *)ctl_be_lun->be_lun;

	memcpy(&lbalen, io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN].bytes,
	       sizeof(lbalen));

	len = lbalen.len * ctl_be_lun->blocksize;

	/*
	 * Kick out the request if it's bigger than we can handle.
	 */
	if (len > softc->rd_size) {
		ctl_set_internal_failure(&io->scsiio,
					 /*sks_valid*/ 0,
					 /*retry_count*/ 0);
		ctl_done(io);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * Kick out the request if it's larger than the device size that
	 * the user requested.
	 */
	if (((lbalen.lba * ctl_be_lun->blocksize) + len) > be_lun->size_bytes) {
		ctl_set_lba_out_of_range(&io->scsiio);
		ctl_done(io);
		return (CTL_RETVAL_COMPLETE);
	}

#ifdef CTL_RAMDISK_PAGES
	num_sg_entries = len >> PAGE_SHIFT;
	if ((len & (PAGE_SIZE - 1)) != 0)
		num_sg_entries++;

	if (num_sg_entries > 1) {
		io->scsiio.kern_data_ptr = malloc(sizeof(struct ctl_sg_entry) *
						  num_sg_entries, M_RAMDISK,
						  M_WAITOK);
		if (io->scsiio.kern_data_ptr == NULL) {
			ctl_set_internal_failure(&io->scsiio,
						 /*sks_valid*/ 0,
						 /*retry_count*/ 0);
			ctl_done(io);
			return (CTL_RETVAL_COMPLETE);
		}
		sg_entries = (struct ctl_sg_entry *)io->scsiio.kern_data_ptr;
		for (i = 0, len_filled = 0; i < num_sg_entries;
		     i++, len_filled += PAGE_SIZE) {
			sg_entries[i].addr = softc->ramdisk_pages[i];
			sg_entries[i].len = ctl_min(PAGE_SIZE,
						    len - len_filled);
		}
	} else {
#endif /* CTL_RAMDISK_PAGES */
		/*
		 * If this is less than 1 page, don't bother allocating a
		 * scatter/gather list for it.  This saves time/overhead.
		 */
		num_sg_entries = 0;
#ifdef CTL_RAMDISK_PAGES
		io->scsiio.kern_data_ptr = softc->ramdisk_pages[0];
#else
		io->scsiio.kern_data_ptr = softc->ramdisk_buffer;
#endif
#ifdef CTL_RAMDISK_PAGES
	}
#endif

	io->scsiio.be_move_done = ctl_backend_ramdisk_move_done;
	io->scsiio.kern_data_len = len;
	io->scsiio.kern_total_len = len;
	io->scsiio.kern_rel_offset = 0;
	io->scsiio.kern_data_resid = 0;
	io->scsiio.kern_sg_entries = num_sg_entries;
	io->io_hdr.flags |= CTL_FLAG_ALLOCATED | CTL_FLAG_KDPTR_SGLIST;
#ifdef CTL_TIME_IO
	getbintime(&io->io_hdr.dma_start_bt);
#endif
	ctl_datamove(io);

	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_backend_ramdisk_ioctl(struct cdev *dev, u_long cmd, caddr_t addr,
			  int flag, struct thread *td)
{
	struct ctl_be_ramdisk_softc *softc;
	int retval;

	retval = 0;
	softc = &rd_softc;

	switch (cmd) {
	case CTL_LUN_REQ: {
		struct ctl_lun_req *lun_req;

		lun_req = (struct ctl_lun_req *)addr;

		switch (lun_req->reqtype) {
		case CTL_LUNREQ_CREATE:
			retval = ctl_backend_ramdisk_create(softc, lun_req,
							    /*do_wait*/ 1);
			break;
		case CTL_LUNREQ_RM:
			retval = ctl_backend_ramdisk_rm(softc, lun_req);
			break;
		case CTL_LUNREQ_MODIFY:
			retval = ctl_backend_ramdisk_modify(softc, lun_req);
			break;
		default:
			lun_req->status = CTL_LUN_ERROR;
			snprintf(lun_req->error_str, sizeof(lun_req->error_str),
				 "%s: invalid LUN request type %d", __func__,
				 lun_req->reqtype);
			break;
		}
		break;
	}
	default:
		retval = ENOTTY;
		break;
	}

	return (retval);
}

static int
ctl_backend_ramdisk_rm(struct ctl_be_ramdisk_softc *softc,
		       struct ctl_lun_req *req)
{
	struct ctl_be_ramdisk_lun *be_lun;
	struct ctl_lun_rm_params *params;
	int retval;


	retval = 0;
	params = &req->reqdata.rm;

	be_lun = NULL;

	mtx_lock(&softc->lock);

	STAILQ_FOREACH(be_lun, &softc->lun_list, links) {
		if (be_lun->ctl_be_lun.lun_id == params->lun_id)
			break;
	}
	mtx_unlock(&softc->lock);

	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: LUN %u is not managed by the ramdisk backend",
			 __func__, params->lun_id);
		goto bailout_error;
	}

	retval = ctl_disable_lun(&be_lun->ctl_be_lun);

	if (retval != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: error %d returned from ctl_disable_lun() for "
			 "LUN %d", __func__, retval, params->lun_id);
		goto bailout_error;
	}

	/*
	 * Set the waiting flag before we invalidate the LUN.  Our shutdown
	 * routine can be called any time after we invalidate the LUN,
	 * and can be called from our context.
	 *
	 * This tells the shutdown routine that we're waiting, or we're
	 * going to wait for the shutdown to happen.
	 */
	mtx_lock(&softc->lock);
	be_lun->flags |= CTL_BE_RAMDISK_LUN_WAITING;
	mtx_unlock(&softc->lock);

	retval = ctl_invalidate_lun(&be_lun->ctl_be_lun);
	if (retval != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: error %d returned from ctl_invalidate_lun() for "
			 "LUN %d", __func__, retval, params->lun_id);
		goto bailout_error;
	}

	mtx_lock(&softc->lock);

	while ((be_lun->flags & CTL_BE_RAMDISK_LUN_UNCONFIGURED) == 0) {
		retval = msleep(be_lun, &softc->lock, PCATCH, "ctlram", 0);
 		if (retval == EINTR)   
			break;
	}
	be_lun->flags &= ~CTL_BE_RAMDISK_LUN_WAITING;

	/*
	 * We only remove this LUN from the list and free it (below) if
	 * retval == 0.  If the user interrupted the wait, we just bail out
	 * without actually freeing the LUN.  We let the shutdown routine
	 * free the LUN if that happens.
	 */
	if (retval == 0) {
		STAILQ_REMOVE(&softc->lun_list, be_lun, ctl_be_ramdisk_lun,
			      links);
		softc->num_luns--;
	}

	mtx_unlock(&softc->lock);

	if (retval == 0)
		free(be_lun, M_RAMDISK);

	req->status = CTL_LUN_OK;

	return (retval);

bailout_error:

	/*
	 * Don't leave the waiting flag set.
	 */
	mtx_lock(&softc->lock);
	be_lun->flags &= ~CTL_BE_RAMDISK_LUN_WAITING;
	mtx_unlock(&softc->lock);

	req->status = CTL_LUN_ERROR;

	return (0);
}

static int
ctl_backend_ramdisk_create(struct ctl_be_ramdisk_softc *softc,
			   struct ctl_lun_req *req, int do_wait)
{
	struct ctl_be_ramdisk_lun *be_lun;
	struct ctl_lun_create_params *params;
	uint32_t blocksize;
	char tmpstr[32];
	int retval;

	retval = 0;
	params = &req->reqdata.create;
	if (params->blocksize_bytes != 0)
		blocksize = params->blocksize_bytes;
	else
		blocksize = 512;

	be_lun = malloc(sizeof(*be_lun), M_RAMDISK, M_ZERO | (do_wait ?
			M_WAITOK : M_NOWAIT));

	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: error allocating %zd bytes", __func__,
			 sizeof(*be_lun));
		goto bailout_error;
	}

	if (params->flags & CTL_LUN_FLAG_DEV_TYPE)
		be_lun->ctl_be_lun.lun_type = params->device_type;
	else
		be_lun->ctl_be_lun.lun_type = T_DIRECT;

	if (be_lun->ctl_be_lun.lun_type == T_DIRECT) {

		if (params->lun_size_bytes < blocksize) {
			snprintf(req->error_str, sizeof(req->error_str),
				 "%s: LUN size %ju < blocksize %u", __func__,
				 params->lun_size_bytes, blocksize);
			goto bailout_error;
		}

		be_lun->size_blocks = params->lun_size_bytes / blocksize;
		be_lun->size_bytes = be_lun->size_blocks * blocksize;

		be_lun->ctl_be_lun.maxlba = be_lun->size_blocks - 1;
	} else {
		be_lun->ctl_be_lun.maxlba = 0;
		blocksize = 0;
		be_lun->size_bytes = 0;
		be_lun->size_blocks = 0;
	}

	be_lun->ctl_be_lun.blocksize = blocksize;

	/* Tell the user the blocksize we ended up using */
	params->blocksize_bytes = blocksize;

	/* Tell the user the exact size we ended up using */
	params->lun_size_bytes = be_lun->size_bytes;

	be_lun->softc = softc;

	be_lun->flags = CTL_BE_RAMDISK_LUN_UNCONFIGURED;
	be_lun->ctl_be_lun.flags = CTL_LUN_FLAG_PRIMARY;
	be_lun->ctl_be_lun.be_lun = be_lun;

	if (params->flags & CTL_LUN_FLAG_ID_REQ) {
		be_lun->ctl_be_lun.req_lun_id = params->req_lun_id;
		be_lun->ctl_be_lun.flags |= CTL_LUN_FLAG_ID_REQ;
	} else
		be_lun->ctl_be_lun.req_lun_id = 0;

	be_lun->ctl_be_lun.lun_shutdown = ctl_backend_ramdisk_lun_shutdown;
	be_lun->ctl_be_lun.lun_config_status =
		ctl_backend_ramdisk_lun_config_status;
	be_lun->ctl_be_lun.be = &ctl_be_ramdisk_driver;
	if ((params->flags & CTL_LUN_FLAG_SERIAL_NUM) == 0) {
		snprintf(tmpstr, sizeof(tmpstr), "MYSERIAL%4d",
			 softc->num_luns);
		strncpy((char *)be_lun->ctl_be_lun.serial_num, tmpstr,
			ctl_min(sizeof(be_lun->ctl_be_lun.serial_num),
			sizeof(tmpstr)));

		/* Tell the user what we used for a serial number */
		strncpy((char *)params->serial_num, tmpstr,
			ctl_min(sizeof(params->serial_num), sizeof(tmpstr)));
	} else { 
		strncpy((char *)be_lun->ctl_be_lun.serial_num,
			params->serial_num,
			ctl_min(sizeof(be_lun->ctl_be_lun.serial_num),
			sizeof(params->serial_num)));
	}
	if ((params->flags & CTL_LUN_FLAG_DEVID) == 0) {
		snprintf(tmpstr, sizeof(tmpstr), "MYDEVID%4d", softc->num_luns);
		strncpy((char *)be_lun->ctl_be_lun.device_id, tmpstr,
			ctl_min(sizeof(be_lun->ctl_be_lun.device_id),
			sizeof(tmpstr)));

		/* Tell the user what we used for a device ID */
		strncpy((char *)params->device_id, tmpstr,
			ctl_min(sizeof(params->device_id), sizeof(tmpstr)));
	} else {
		strncpy((char *)be_lun->ctl_be_lun.device_id,
			params->device_id,
			ctl_min(sizeof(be_lun->ctl_be_lun.device_id),
				sizeof(params->device_id)));
	}

	mtx_lock(&softc->lock);
	softc->num_luns++;
	STAILQ_INSERT_TAIL(&softc->lun_list, be_lun, links);

	mtx_unlock(&softc->lock);

	retval = ctl_add_lun(&be_lun->ctl_be_lun);
	if (retval != 0) {
		mtx_lock(&softc->lock);
		STAILQ_REMOVE(&softc->lun_list, be_lun, ctl_be_ramdisk_lun,
			      links);
		softc->num_luns--;
		mtx_unlock(&softc->lock);
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: ctl_add_lun() returned error %d, see dmesg for "
			"details", __func__, retval);
		retval = 0;
		goto bailout_error;
	}

	if (do_wait == 0)
		return (retval);

	mtx_lock(&softc->lock);

	/*
	 * Tell the config_status routine that we're waiting so it won't
	 * clean up the LUN in the event of an error.
	 */
	be_lun->flags |= CTL_BE_RAMDISK_LUN_WAITING;

	while (be_lun->flags & CTL_BE_RAMDISK_LUN_UNCONFIGURED) {
		retval = msleep(be_lun, &softc->lock, PCATCH, "ctlram", 0);
		if (retval == EINTR)
			break;
	}
	be_lun->flags &= ~CTL_BE_RAMDISK_LUN_WAITING;

	if (be_lun->flags & CTL_BE_RAMDISK_LUN_CONFIG_ERR) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: LUN configuration error, see dmesg for details",
			 __func__);
		STAILQ_REMOVE(&softc->lun_list, be_lun, ctl_be_ramdisk_lun,
			      links);
		softc->num_luns--;
		mtx_unlock(&softc->lock);
		goto bailout_error;
	} else {
		params->req_lun_id = be_lun->ctl_be_lun.lun_id;
	}
	mtx_unlock(&softc->lock);

	req->status = CTL_LUN_OK;

	return (retval);

bailout_error:
	req->status = CTL_LUN_ERROR;
	free(be_lun, M_RAMDISK);

	return (retval);
}

static int
ctl_backend_ramdisk_modify(struct ctl_be_ramdisk_softc *softc,
		       struct ctl_lun_req *req)
{
	struct ctl_be_ramdisk_lun *be_lun;
	struct ctl_lun_modify_params *params;
	uint32_t blocksize;

	params = &req->reqdata.modify;

	be_lun = NULL;

	mtx_lock(&softc->lock);
	STAILQ_FOREACH(be_lun, &softc->lun_list, links) {
		if (be_lun->ctl_be_lun.lun_id == params->lun_id)
			break;
	}
	mtx_unlock(&softc->lock);

	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: LUN %u is not managed by the ramdisk backend",
			 __func__, params->lun_id);
		goto bailout_error;
	}

	if (params->lun_size_bytes == 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			"%s: LUN size \"auto\" not supported "
			"by the ramdisk backend", __func__);
		goto bailout_error;
	}

	blocksize = be_lun->ctl_be_lun.blocksize;

	if (params->lun_size_bytes < blocksize) {
		snprintf(req->error_str, sizeof(req->error_str),
			"%s: LUN size %ju < blocksize %u", __func__,
			params->lun_size_bytes, blocksize);
		goto bailout_error;
	}

	be_lun->size_blocks = params->lun_size_bytes / blocksize;
	be_lun->size_bytes = be_lun->size_blocks * blocksize;

	/*
	 * The maximum LBA is the size - 1.
	 *
	 * XXX: Note that this field is being updated without locking,
	 * 	which might cause problems on 32-bit architectures.
	 */
	be_lun->ctl_be_lun.maxlba = be_lun->size_blocks - 1;
	ctl_lun_capacity_changed(&be_lun->ctl_be_lun);

	/* Tell the user the exact size we ended up using */
	params->lun_size_bytes = be_lun->size_bytes;

	req->status = CTL_LUN_OK;

	return (0);

bailout_error:
	req->status = CTL_LUN_ERROR;

	return (0);
}

static void
ctl_backend_ramdisk_lun_shutdown(void *be_lun)
{
	struct ctl_be_ramdisk_lun *lun;
	struct ctl_be_ramdisk_softc *softc;
	int do_free;

	lun = (struct ctl_be_ramdisk_lun *)be_lun;
	softc = lun->softc;
	do_free = 0;

	mtx_lock(&softc->lock);

	lun->flags |= CTL_BE_RAMDISK_LUN_UNCONFIGURED;

	if (lun->flags & CTL_BE_RAMDISK_LUN_WAITING) {
		wakeup(lun);
	} else {
		STAILQ_REMOVE(&softc->lun_list, be_lun, ctl_be_ramdisk_lun,
			      links);
		softc->num_luns--;
		do_free = 1;
	}

	mtx_unlock(&softc->lock);

	if (do_free != 0)
		free(be_lun, M_RAMDISK);
}

static void
ctl_backend_ramdisk_lun_config_status(void *be_lun,
				      ctl_lun_config_status status)
{
	struct ctl_be_ramdisk_lun *lun;
	struct ctl_be_ramdisk_softc *softc;

	lun = (struct ctl_be_ramdisk_lun *)be_lun;
	softc = lun->softc;

	if (status == CTL_LUN_CONFIG_OK) {
		mtx_lock(&softc->lock);
		lun->flags &= ~CTL_BE_RAMDISK_LUN_UNCONFIGURED;
		if (lun->flags & CTL_BE_RAMDISK_LUN_WAITING)
			wakeup(lun);
		mtx_unlock(&softc->lock);

		/*
		 * We successfully added the LUN, attempt to enable it.
		 */
		if (ctl_enable_lun(&lun->ctl_be_lun) != 0) {
			printf("%s: ctl_enable_lun() failed!\n", __func__);
			if (ctl_invalidate_lun(&lun->ctl_be_lun) != 0) {
				printf("%s: ctl_invalidate_lun() failed!\n",
				       __func__);
			}
		}

		return;
	}


	mtx_lock(&softc->lock);
	lun->flags &= ~CTL_BE_RAMDISK_LUN_UNCONFIGURED;

	/*
	 * If we have a user waiting, let him handle the cleanup.  If not,
	 * clean things up here.
	 */
	if (lun->flags & CTL_BE_RAMDISK_LUN_WAITING) {
		lun->flags |= CTL_BE_RAMDISK_LUN_CONFIG_ERR;
		wakeup(lun);
	} else {
		STAILQ_REMOVE(&softc->lun_list, lun, ctl_be_ramdisk_lun,
			      links);
		softc->num_luns--;
		free(lun, M_RAMDISK);
	}
	mtx_unlock(&softc->lock);
}

static int
ctl_backend_ramdisk_config_write(union ctl_io *io)
{
	struct ctl_be_ramdisk_softc *softc;
	int retval;

	retval = 0;
	softc = &rd_softc;

	switch (io->scsiio.cdb[0]) {
	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
		/*
		 * The upper level CTL code will filter out any CDBs with
		 * the immediate bit set and return the proper error.  It
		 * will also not allow a sync cache command to go to a LUN
		 * that is powered down.
		 *
		 * We don't really need to worry about what LBA range the
		 * user asked to be synced out.  When they issue a sync
		 * cache command, we'll sync out the whole thing.
		 *
		 * This is obviously just a stubbed out implementation.
		 * The real implementation will be in the RAIDCore/CTL
		 * interface, and can only really happen when RAIDCore
		 * implements a per-array cache sync.
		 */
		ctl_set_success(&io->scsiio);
		ctl_config_write_done(io);
		break;
	case START_STOP_UNIT: {
		struct scsi_start_stop_unit *cdb;
		struct ctl_be_lun *ctl_be_lun;
		struct ctl_be_ramdisk_lun *be_lun;

		cdb = (struct scsi_start_stop_unit *)io->scsiio.cdb;

		ctl_be_lun = (struct ctl_be_lun *)io->io_hdr.ctl_private[
			CTL_PRIV_BACKEND_LUN].ptr;
		be_lun = (struct ctl_be_ramdisk_lun *)ctl_be_lun->be_lun;

		if (cdb->how & SSS_START)
			retval = ctl_start_lun(ctl_be_lun);
		else {
			retval = ctl_stop_lun(ctl_be_lun);
#ifdef NEEDTOPORT
			if ((retval == 0)
			 && (cdb->byte2 & SSS_ONOFFLINE))
				retval = ctl_lun_offline(ctl_be_lun);
#endif
		}

		/*
		 * In general, the above routines should not fail.  They
		 * just set state for the LUN.  So we've got something
		 * pretty wrong here if we can't start or stop the LUN.
		 */
		if (retval != 0) {
			ctl_set_internal_failure(&io->scsiio,
						 /*sks_valid*/ 1,
						 /*retry_count*/ 0xf051);
			retval = CTL_RETVAL_COMPLETE;
		} else {
			ctl_set_success(&io->scsiio);
		}
		ctl_config_write_done(io);
		break;
	}
	default:
		ctl_set_invalid_opcode(&io->scsiio);
		ctl_config_write_done(io);
		retval = CTL_RETVAL_COMPLETE;
		break;
	}

	return (retval);
}

static int
ctl_backend_ramdisk_config_read(union ctl_io *io)
{
	/*
	 * XXX KDM need to implement!!
	 */
	return (0);
}
