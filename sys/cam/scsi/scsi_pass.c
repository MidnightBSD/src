/*-
 * Copyright (c) 1997, 1998, 2000 Justin T. Gibbs.
 * Copyright (c) 1997, 1998, 1999 Kenneth D. Merry.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/cam/scsi/scsi_pass.c 169605 2007-05-16 16:54:23Z scottl $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/devicestat.h>
#include <sys/proc.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_pass.h>

typedef enum {
	PASS_FLAG_OPEN			= 0x01,
	PASS_FLAG_LOCKED		= 0x02,
	PASS_FLAG_INVALID		= 0x04
} pass_flags;

typedef enum {
	PASS_STATE_NORMAL
} pass_state;

typedef enum {
	PASS_CCB_BUFFER_IO,
	PASS_CCB_WAITING
} pass_ccb_types;

#define ccb_type	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct pass_softc {
	pass_state		state;
	pass_flags		flags;
	u_int8_t		pd_type;
	union ccb		saved_ccb;
	struct devstat		*device_stats;
	struct cdev *dev;
};


static	d_open_t	passopen;
static	d_close_t	passclose;
static	d_ioctl_t	passioctl;

static	periph_init_t	passinit;
static	periph_ctor_t	passregister;
static	periph_oninv_t	passoninvalidate;
static	periph_dtor_t	passcleanup;
static	periph_start_t	passstart;
static	void		passasync(void *callback_arg, u_int32_t code,
				  struct cam_path *path, void *arg);
static	void		passdone(struct cam_periph *periph, 
				 union ccb *done_ccb);
static	int		passerror(union ccb *ccb, u_int32_t cam_flags, 
				  u_int32_t sense_flags);
static 	int		passsendccb(struct cam_periph *periph, union ccb *ccb,
				    union ccb *inccb);

static struct periph_driver passdriver =
{
	passinit, "pass",
	TAILQ_HEAD_INITIALIZER(passdriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(pass, passdriver);

static struct cdevsw pass_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	passopen,
	.d_close =	passclose,
	.d_ioctl =	passioctl,
	.d_name =	"pass",
};

static void
passinit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, passasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("pass: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
	
}

static void
passoninvalidate(struct cam_periph *periph)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, passasync, periph, periph->path);

	softc->flags |= PASS_FLAG_INVALID;

	/*
	 * XXX Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */

	if (bootverbose) {
		xpt_print(periph->path, "lost device\n");
	}

}

static void
passcleanup(struct cam_periph *periph)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	devstat_remove_entry(softc->device_stats);

	destroy_dev(softc->dev);

	if (bootverbose) {
		xpt_print(periph->path, "removing device entry\n");
	}
	free(softc, M_DEVBUF);
}

static void
passasync(void *callback_arg, u_int32_t code,
	  struct cam_path *path, void *arg)
{
	struct cam_periph *periph;
	struct cam_sim *sim;

	periph = (struct cam_periph *)callback_arg;

	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;
		if (cgd == NULL)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		sim = xpt_path_sim(cgd->ccb_h.path);
		status = cam_periph_alloc(passregister, passoninvalidate,
					  passcleanup, passstart, "pass",
					  CAM_PERIPH_BIO, cgd->ccb_h.path,
					  passasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG) {
			const struct cam_status_entry *entry;

			entry = cam_fetch_status_entry(status);

			printf("passasync: Unable to attach new device "
			       "due to status %#x: %s\n", status, entry ?
			       entry->status_text : "Unknown");
		}

		break;
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static cam_status
passregister(struct cam_periph *periph, void *arg)
{
	struct pass_softc *softc;
	struct ccb_getdev *cgd;
	int    no_tags;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("passregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("passregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct pass_softc *)malloc(sizeof(*softc),
					    M_DEVBUF, M_NOWAIT);

	if (softc == NULL) {
		printf("passregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	softc->state = PASS_STATE_NORMAL;
	softc->pd_type = SID_TYPE(&cgd->inq_data);

	periph->softc = softc;

	/*
	 * We pass in 0 for a blocksize, since we don't 
	 * know what the blocksize of this device is, if 
	 * it even has a blocksize.
	 */
	mtx_unlock(periph->sim->mtx);
	no_tags = (cgd->inq_data.flags & SID_CmdQue) == 0;
	softc->device_stats = devstat_new_entry("pass",
			  unit2minor(periph->unit_number), 0,
			  DEVSTAT_NO_BLOCKSIZE
			  | (no_tags ? DEVSTAT_NO_ORDERED_TAGS : 0),
			  softc->pd_type |
			  DEVSTAT_TYPE_IF_SCSI |
			  DEVSTAT_TYPE_PASS,
			  DEVSTAT_PRIORITY_PASS);

	/* Register the device */
	softc->dev = make_dev(&pass_cdevsw, unit2minor(periph->unit_number),
			      UID_ROOT, GID_OPERATOR, 0600, "%s%d",
			      periph->periph_name, periph->unit_number);
	mtx_lock(periph->sim->mtx);
	softc->dev->si_drv1 = periph;

	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_register_async(AC_LOST_DEVICE, passasync, periph, periph->path);

	if (bootverbose)
		xpt_announce_periph(periph, NULL);

	return(CAM_REQ_CMP);
}

static int
passopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct pass_softc *softc;
	int error;

	error = 0; /* default to no error */

	periph = (struct cam_periph *)dev->si_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return (ENXIO);

	cam_periph_lock(periph);

	softc = (struct pass_softc *)periph->softc;

	if (softc->flags & PASS_FLAG_INVALID) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return(ENXIO);
	}

	/*
	 * Don't allow access when we're running at a high securelevel.
	 */
	error = securelevel_gt(td->td_ucred, 1);
	if (error) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return(error);
	}

	/*
	 * Only allow read-write access.
	 */
	if (((flags & FWRITE) == 0) || ((flags & FREAD) == 0)) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return(EPERM);
	}

	/*
	 * We don't allow nonblocking access.
	 */
	if ((flags & O_NONBLOCK) != 0) {
		xpt_print(periph->path, "can't do nonblocking access\n");
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return(EINVAL);
	}

	if ((softc->flags & PASS_FLAG_OPEN) == 0) {
		softc->flags |= PASS_FLAG_OPEN;
	} else {
		/* Device closes aren't symmertical, so fix up the refcount */
		cam_periph_release(periph);
	}

	cam_periph_unlock(periph);

	return (error);
}

static int
passclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct 	cam_periph *periph;
	struct	pass_softc *softc;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return (ENXIO);	

	cam_periph_lock(periph);

	softc = (struct pass_softc *)periph->softc;
	softc->flags &= ~PASS_FLAG_OPEN;

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (0);
}

static void
passstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	switch (softc->state) {
	case PASS_STATE_NORMAL:
		start_ccb->ccb_h.ccb_type = PASS_CCB_WAITING;			
		SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
				  periph_links.sle);
		periph->immediate_priority = CAM_PRIORITY_NONE;
		wakeup(&periph->ccb_list);
		break;
	}
}

static void
passdone(struct cam_periph *periph, union ccb *done_ccb)
{ 
	struct pass_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct pass_softc *)periph->softc;
	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_type) {
	case PASS_CCB_WAITING:
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	xpt_release_ccb(done_ccb);
}

static int
passioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct	cam_periph *periph;
	struct	pass_softc *softc;
	int	error;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return(ENXIO);

	cam_periph_lock(periph);
	softc = (struct pass_softc *)periph->softc;

	error = 0;

	switch (cmd) {

	case CAMIOCOMMAND:
	{
		union ccb *inccb;
		union ccb *ccb;
		int ccb_malloced;

		inccb = (union ccb *)addr;

		/*
		 * Some CCB types, like scan bus and scan lun can only go
		 * through the transport layer device.
		 */
		if (inccb->ccb_h.func_code & XPT_FC_XPT_ONLY) {
			xpt_print(periph->path, "CCB function code %#x is "
			    "restricted to the XPT device\n",
			    inccb->ccb_h.func_code);
			error = ENODEV;
			break;
		}

		/*
		 * Non-immediate CCBs need a CCB from the per-device pool
		 * of CCBs, which is scheduled by the transport layer.
		 * Immediate CCBs and user-supplied CCBs should just be
		 * malloced.
		 */
		if ((inccb->ccb_h.func_code & XPT_FC_QUEUED)
		 && ((inccb->ccb_h.func_code & XPT_FC_USER_CCB) == 0)) {
			ccb = cam_periph_getccb(periph,
						inccb->ccb_h.pinfo.priority);
			ccb_malloced = 0;
		} else {
			ccb = xpt_alloc_ccb_nowait();

			if (ccb != NULL)
				xpt_setup_ccb(&ccb->ccb_h, periph->path,
					      inccb->ccb_h.pinfo.priority);
			ccb_malloced = 1;
		}

		if (ccb == NULL) {
			xpt_print(periph->path, "unable to allocate CCB\n");
			error = ENOMEM;
			break;
		}

		error = passsendccb(periph, ccb, inccb);

		if (ccb_malloced)
			xpt_free_ccb(ccb);
		else
			xpt_release_ccb(ccb);

		break;
	}
	default:
		error = cam_periph_ioctl(periph, cmd, addr, passerror);
		break;
	}

	cam_periph_unlock(periph);
	return(error);
}

/*
 * Generally, "ccb" should be the CCB supplied by the kernel.  "inccb"
 * should be the CCB that is copied in from the user.
 */
static int
passsendccb(struct cam_periph *periph, union ccb *ccb, union ccb *inccb)
{
	struct pass_softc *softc;
	struct cam_periph_map_info mapinfo;
	int error, need_unmap;

	softc = (struct pass_softc *)periph->softc;

	need_unmap = 0;

	/*
	 * There are some fields in the CCB header that need to be
	 * preserved, the rest we get from the user.
	 */
	xpt_merge_ccb(ccb, inccb);

	/*
	 * There's no way for the user to have a completion
	 * function, so we put our own completion function in here.
	 */
	ccb->ccb_h.cbfcnp = passdone;

	/*
	 * We only attempt to map the user memory into kernel space
	 * if they haven't passed in a physical memory pointer,
	 * and if there is actually an I/O operation to perform.
	 * Right now cam_periph_mapmem() only supports SCSI and device
	 * match CCBs.  For the SCSI CCBs, we only pass the CCB in if
	 * there's actually data to map.  cam_periph_mapmem() will do the
	 * right thing, even if there isn't data to map, but since CCBs
	 * without data are a reasonably common occurance (e.g. test unit
	 * ready), it will save a few cycles if we check for it here.
	 */
	if (((ccb->ccb_h.flags & CAM_DATA_PHYS) == 0)
	 && (((ccb->ccb_h.func_code == XPT_SCSI_IO)
	    && ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE))
	  || (ccb->ccb_h.func_code == XPT_DEV_MATCH))) {

		bzero(&mapinfo, sizeof(mapinfo));

		/*
		 * cam_periph_mapmem calls into proc and vm functions that can
		 * sleep as well as trigger I/O, so we can't hold the lock.
		 * Dropping it here is reasonably safe.
		 */
		cam_periph_unlock(periph);
		error = cam_periph_mapmem(ccb, &mapinfo); 
		cam_periph_lock(periph);

		/*
		 * cam_periph_mapmem returned an error, we can't continue.
		 * Return the error to the user.
		 */
		if (error)
			return(error);

		/*
		 * We successfully mapped the memory in, so we need to
		 * unmap it when the transaction is done.
		 */
		need_unmap = 1;
	}

	/*
	 * If the user wants us to perform any error recovery, then honor
	 * that request.  Otherwise, it's up to the user to perform any
	 * error recovery.
	 */
	error = cam_periph_runccb(ccb,
				  (ccb->ccb_h.flags & CAM_PASS_ERR_RECOVER) ?
				  passerror : NULL,
				  /* cam_flags */ CAM_RETRY_SELTO,
				  /* sense_flags */SF_RETRY_UA,
				  softc->device_stats);

	if (need_unmap != 0)
		cam_periph_unmapmem(ccb, &mapinfo);

	ccb->ccb_h.cbfcnp = NULL;
	ccb->ccb_h.periph_priv = inccb->ccb_h.periph_priv;
	bcopy(ccb, inccb, sizeof(union ccb));

	return(error);
}

static int
passerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct cam_periph *periph;
	struct pass_softc *softc;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct pass_softc *)periph->softc;
	
	return(cam_periph_error(ccb, cam_flags, sense_flags, 
				 &softc->saved_ccb));
}
