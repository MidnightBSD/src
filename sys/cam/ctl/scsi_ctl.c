/*-
 * Copyright (c) 2008, 2009 Silicon Graphics International Corp.
 * All rights reserved.
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
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/scsi_ctl.c#4 $
 */
/*
 * Peripheral driver interface between CAM and CTL (CAM Target Layer).
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_error.h>

typedef enum {
	CTLFE_CCB_WAITING 	= 0x01
} ctlfe_ccb_types;

struct ctlfe_softc {
	struct ctl_frontend fe;
	path_id_t path_id;
	struct cam_sim *sim;
	char port_name[DEV_IDLEN];
	STAILQ_HEAD(, ctlfe_lun_softc) lun_softc_list;
	STAILQ_ENTRY(ctlfe_softc) links;
};

STAILQ_HEAD(, ctlfe_softc) ctlfe_softc_list;
struct mtx ctlfe_list_mtx;
static char ctlfe_mtx_desc[] = "ctlfelist";
static int ctlfe_dma_enabled = 1;
#ifdef CTLFE_INIT_ENABLE
static int ctlfe_max_targets = 1;
static int ctlfe_num_targets = 0;
#endif

typedef enum {
	CTLFE_LUN_NONE		= 0x00,
	CTLFE_LUN_WILDCARD	= 0x01
} ctlfe_lun_flags;

struct ctlfe_lun_softc {
	struct ctlfe_softc *parent_softc;
	struct cam_periph *periph;
	ctlfe_lun_flags flags;
	struct callout dma_callout;
	uint64_t ccbs_alloced;
	uint64_t ccbs_freed;
	uint64_t ctios_sent;
	uint64_t ctios_returned;
	uint64_t atios_sent;
	uint64_t atios_returned;
	uint64_t inots_sent;
	uint64_t inots_returned;
	/* bus_dma_tag_t dma_tag; */
	TAILQ_HEAD(, ccb_hdr) work_queue;
	STAILQ_ENTRY(ctlfe_lun_softc) links;
};

typedef enum {
	CTLFE_CMD_NONE		= 0x00,
	CTLFE_CMD_PIECEWISE	= 0x01
} ctlfe_cmd_flags;

/*
 * The size limit of this structure is CTL_PORT_PRIV_SIZE, from ctl_io.h.
 * Currently that is 600 bytes.
 */
struct ctlfe_lun_cmd_info {
	int cur_transfer_index;
	ctlfe_cmd_flags flags;
	/*
	 * XXX KDM struct bus_dma_segment is 8 bytes on i386, and 16
	 * bytes on amd64.  So with 32 elements, this is 256 bytes on
	 * i386 and 512 bytes on amd64.
	 */
	bus_dma_segment_t cam_sglist[32];
};

/*
 * When we register the adapter/bus, request that this many ctl_ios be
 * allocated.  This should be the maximum supported by the adapter, but we
 * currently don't have a way to get that back from the path inquiry.
 * XXX KDM add that to the path inquiry.
 */
#define	CTLFE_REQ_CTL_IO	4096
/*
 * Number of Accept Target I/O CCBs to allocate and queue down to the
 * adapter per LUN.
 * XXX KDM should this be controlled by CTL?
 */
#define	CTLFE_ATIO_PER_LUN	1024
/*
 * Number of Immediate Notify CCBs (used for aborts, resets, etc.) to
 * allocate and queue down to the adapter per LUN.
 * XXX KDM should this be controlled by CTL?
 */
#define	CTLFE_IN_PER_LUN	1024

/*
 * Timeout (in seconds) on CTIO CCB allocation for doing a DMA or sending
 * status to the initiator.  The SIM is expected to have its own timeouts,
 * so we're not putting this timeout around the CCB execution time.  The
 * SIM should timeout and let us know if it has an issue.
 */
#define	CTLFE_DMA_TIMEOUT	60

/*
 * Turn this on to enable extra debugging prints.
 */
#if 0
#define	CTLFE_DEBUG
#endif

/*
 * Use randomly assigned WWNN/WWPN values.  This is to work around an issue
 * in the FreeBSD initiator that makes it unable to rescan the target if
 * the target gets rebooted and the WWNN/WWPN stay the same.
 */
#if 0
#define	RANDOM_WWNN
#endif

SYSCTL_INT(_kern_cam_ctl, OID_AUTO, dma_enabled, CTLFLAG_RW,
	   &ctlfe_dma_enabled, 0, "DMA enabled");
MALLOC_DEFINE(M_CTLFE, "CAM CTL FE", "CAM CTL FE interface");

#define	ccb_type	ppriv_field0
/* This is only used in the ATIO */
#define	io_ptr		ppriv_ptr1

/* This is only used in the CTIO */
#define	ccb_atio	ppriv_ptr1

int			ctlfeinitialize(void);
void			ctlfeshutdown(void);
static periph_init_t	ctlfeinit;
static void		ctlfeasync(void *callback_arg, uint32_t code,
				   struct cam_path *path, void *arg);
static periph_ctor_t	ctlferegister;
static periph_oninv_t	ctlfeoninvalidate;
static periph_dtor_t	ctlfecleanup;
static periph_start_t	ctlfestart;
static void		ctlfedone(struct cam_periph *periph,
				  union ccb *done_ccb);

static void 		ctlfe_onoffline(void *arg, int online);
static void 		ctlfe_online(void *arg);
static void 		ctlfe_offline(void *arg);
static int 		ctlfe_targ_enable(void *arg, struct ctl_id targ_id);
static int 		ctlfe_targ_disable(void *arg, struct ctl_id targ_id);
static int 		ctlfe_lun_enable(void *arg, struct ctl_id targ_id,
					 int lun_id);
static int 		ctlfe_lun_disable(void *arg, struct ctl_id targ_id,
					  int lun_id);
static void		ctlfe_dump_sim(struct cam_sim *sim);
static void		ctlfe_dump_queue(struct ctlfe_lun_softc *softc);
static void		ctlfe_dma_timeout(void *arg);
static void 		ctlfe_datamove_done(union ctl_io *io);
static void 		ctlfe_dump(void);

static struct periph_driver ctlfe_driver =
{
	ctlfeinit, "ctl",
	TAILQ_HEAD_INITIALIZER(ctlfe_driver.units), /*generation*/ 0
};
PERIPHDRIVER_DECLARE(ctl, ctlfe_driver);

extern struct ctl_softc *control_softc;
extern int ctl_disable;

int
ctlfeinitialize(void)
{
	cam_status status;

	/* Don't initialize if we're disabled */
	if (ctl_disable != 0)
		return (0);

	STAILQ_INIT(&ctlfe_softc_list);

	mtx_init(&ctlfe_list_mtx, ctlfe_mtx_desc, NULL, MTX_DEF);

	xpt_lock_buses();
	periphdriver_register(&ctlfe_driver);
	xpt_unlock_buses();

	status = xpt_register_async(AC_PATH_REGISTERED | AC_PATH_DEREGISTERED | 
				    AC_CONTRACT, ctlfeasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("ctl: Failed to attach async callback due to CAM "
		       "status 0x%x!\n", status);
	}

	return (0);
}

void
ctlfeshutdown(void)
{
	return;
}

void
ctlfeinit(void)
{
	cam_status status;

	/* Don't initialize if we're disabled */
	if (ctl_disable != 0)
		return;

	STAILQ_INIT(&ctlfe_softc_list);

	mtx_init(&ctlfe_list_mtx, ctlfe_mtx_desc, NULL, MTX_DEF);

	KASSERT(control_softc != NULL, ("CTL is not initialized!"));

	status = xpt_register_async(AC_PATH_REGISTERED | AC_PATH_DEREGISTERED |
				    AC_CONTRACT, ctlfeasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("ctl: Failed to attach async callback due to CAM "
		       "status 0x%x!\n", status);
	}
}

static void
ctlfeasync(void *callback_arg, uint32_t code, struct cam_path *path, void *arg)
{

#ifdef CTLFEDEBUG
	printf("%s: entered\n", __func__);
#endif

	/*
	 * When a new path gets registered, and it is capable of target
	 * mode, go ahead and attach.  Later on, we may need to be more
	 * selective, but for now this will be sufficient.
 	 */
	switch (code) {
	case AC_PATH_REGISTERED: {
		struct ctl_frontend *fe;
		struct ctlfe_softc *bus_softc;
		struct ctlfe_lun_softc *lun_softc;
		struct cam_path *path;
		struct ccb_pathinq *cpi;
		cam_status status;
		int retval;

		cpi = (struct ccb_pathinq *)arg;

		/* Don't attach if it doesn't support target mode */
		if ((cpi->target_sprt & PIT_PROCESSOR) == 0) {
#ifdef CTLFEDEBUG
			printf("%s: SIM %s%d doesn't support target mode\n",
			       __func__, cpi->dev_name, cpi->unit_number);
#endif
			break;
		}

#ifdef CTLFE_INIT_ENABLE
		if (ctlfe_num_targets >= ctlfe_max_targets) {
			union ccb *ccb;
			struct cam_sim *sim;

			ccb = (union ccb *)malloc(sizeof(*ccb), M_TEMP,
						  M_NOWAIT | M_ZERO);
			if (ccb == NULL) {
				printf("%s: unable to malloc CCB!\n", __func__);
				xpt_free_path(path);
				return;
			}
			xpt_setup_ccb(&ccb->ccb_h, cpi->ccb_h.path,
				      /*priority*/ 1);

			sim = xpt_path_sim(cpi->ccb_h.path);

			ccb->ccb_h.func_code = XPT_SET_SIM_KNOB;
			ccb->knob.xport_specific.valid = KNOB_VALID_ROLE;
			ccb->knob.xport_specific.fc.role = KNOB_ROLE_INITIATOR;

			/* We should hold the SIM lock here */
			mtx_assert(sim->mtx, MA_OWNED);

			xpt_action(ccb);

			if ((ccb->ccb_h.status & CAM_STATUS_MASK) !=
			     CAM_REQ_CMP) {
				printf("%s: SIM %s%d (path id %d) initiator "
				       "enable failed with status %#x\n",
				       __func__, cpi->dev_name,
				       cpi->unit_number, cpi->ccb_h.path_id,
				       ccb->ccb_h.status);
			} else {
				printf("%s: SIM %s%d (path id %d) initiator "
				       "enable succeeded\n",
				       __func__, cpi->dev_name,
				       cpi->unit_number, cpi->ccb_h.path_id);
			}

			free(ccb, M_TEMP);

			break;
		} else {
			ctlfe_num_targets++;
		}

		printf("%s: ctlfe_num_targets = %d\n", __func__,
		       ctlfe_num_targets);
#endif /* CTLFE_INIT_ENABLE */

		/*
		 * We're in an interrupt context here, so we have to
		 * use M_NOWAIT.  Of course this means trouble if we
		 * can't allocate memory.
		 */
		bus_softc = malloc(sizeof(*bus_softc), M_CTLFE,
				   M_NOWAIT | M_ZERO);
		if (bus_softc == NULL) {
			printf("%s: unable to malloc %zd bytes for softc\n",
			       __func__, sizeof(*bus_softc));
			return;
		}

		bus_softc->path_id = cpi->ccb_h.path_id;
		bus_softc->sim = xpt_path_sim(cpi->ccb_h.path);
		STAILQ_INIT(&bus_softc->lun_softc_list);

		fe = &bus_softc->fe;

		/*
		 * XXX KDM should we be more accurate here ?
		 */
		if (cpi->transport == XPORT_FC)
			fe->port_type = CTL_PORT_FC;
		else
			fe->port_type = CTL_PORT_SCSI;

		/* XXX KDM what should the real number be here? */
		fe->num_requested_ctl_io = 4096;
		snprintf(bus_softc->port_name, sizeof(bus_softc->port_name),
			 "%s%d", cpi->dev_name, cpi->unit_number);
		/*
		 * XXX KDM it would be nice to allocate storage in the
		 * frontend structure itself.
	 	 */
		fe->port_name = bus_softc->port_name;
		fe->physical_port = cpi->unit_number;
		fe->virtual_port = cpi->bus_id;
		fe->port_online = ctlfe_online;
		fe->port_offline = ctlfe_offline;
		fe->onoff_arg = bus_softc;
		fe->targ_enable = ctlfe_targ_enable;
		fe->targ_disable = ctlfe_targ_disable;
		fe->lun_enable = ctlfe_lun_enable;
		fe->lun_disable = ctlfe_lun_disable;
		fe->targ_lun_arg = bus_softc;
		fe->fe_datamove = ctlfe_datamove_done;
		fe->fe_done = ctlfe_datamove_done;
		fe->fe_dump = ctlfe_dump;
		/*
		 * XXX KDM the path inquiry doesn't give us the maximum
		 * number of targets supported.
		 */
		fe->max_targets = cpi->max_target;
		fe->max_target_id = cpi->max_target;
		
		/*
		 * XXX KDM need to figure out whether we're the master or
		 * slave.
		 */
#ifdef CTLFEDEBUG
		printf("%s: calling ctl_frontend_register() for %s%d\n",
		       __func__, cpi->dev_name, cpi->unit_number);
#endif
		retval = ctl_frontend_register(fe, /*master_SC*/ 1);
		if (retval != 0) {
			printf("%s: ctl_frontend_register() failed with "
			       "error %d!\n", __func__, retval);
			free(bus_softc, M_CTLFE);
			break;
		} else {
			mtx_lock(&ctlfe_list_mtx);
			STAILQ_INSERT_TAIL(&ctlfe_softc_list, bus_softc, links);
			mtx_unlock(&ctlfe_list_mtx);
		}

		status = xpt_create_path(&path, /*periph*/ NULL,
					 bus_softc->path_id,CAM_TARGET_WILDCARD,
					 CAM_LUN_WILDCARD);
		if (status != CAM_REQ_CMP) {
			printf("%s: unable to create path for wildcard "
			       "periph\n", __func__);
			break;
		}
		lun_softc = malloc(sizeof(*lun_softc), M_CTLFE,
				   M_NOWAIT | M_ZERO);
		if (lun_softc == NULL) {
			xpt_print(path, "%s: unable to allocate softc for "
				  "wildcard periph\n", __func__);
			xpt_free_path(path);
			break;
		}

		lun_softc->parent_softc = bus_softc;
		lun_softc->flags |= CTLFE_LUN_WILDCARD;

		status = cam_periph_alloc(ctlferegister,
					  ctlfeoninvalidate,
					  ctlfecleanup,
					  ctlfestart,
					  "ctl",
					  CAM_PERIPH_BIO,
					  path,
					  ctlfeasync,
					  0,
					  lun_softc);

		xpt_free_path(path);

		break;
	}
	case AC_PATH_DEREGISTERED:
		/* ctl_frontend_deregister() */
		break;
	case AC_CONTRACT: {
		struct ac_contract *ac;

		ac = (struct ac_contract *)arg;

		switch (ac->contract_number) {
		case AC_CONTRACT_DEV_CHG: {
			struct ac_device_changed *dev_chg;
			struct ctlfe_softc *softc;
			int retval, found;

			dev_chg = (struct ac_device_changed *)ac->contract_data;

			printf("%s: WWPN %#jx port %u path %u target %u %s\n",
			       __func__, dev_chg->wwpn, dev_chg->port,
			       xpt_path_path_id(path), dev_chg->target,
			       (dev_chg->arrived == 0) ?  "left" : "arrived");

			found = 0;

			mtx_lock(&ctlfe_list_mtx);
			STAILQ_FOREACH(softc, &ctlfe_softc_list, links) {
				if (softc->path_id == xpt_path_path_id(path)) {
					found = 1;
					break;
				}
			}
			mtx_unlock(&ctlfe_list_mtx);

			if (found == 0) {
				printf("%s: CTL port for CAM path %u not "
				       "found!\n", __func__,
				       xpt_path_path_id(path));
				break;
			}
			if (dev_chg->arrived != 0) {
				retval = ctl_add_initiator(dev_chg->wwpn,
					softc->fe.targ_port, dev_chg->target);
			} else {
				retval = ctl_remove_initiator(
					softc->fe.targ_port, dev_chg->target);
			}

			if (retval != 0) {
				printf("%s: could not %s port %d iid %u "
				       "WWPN %#jx!\n", __func__,
				       (dev_chg->arrived != 0) ? "add" :
				       "remove", softc->fe.targ_port,
				       dev_chg->target,
				       (uintmax_t)dev_chg->wwpn);
			}
			break;
		}
		default:
			printf("%s: unsupported contract number %ju\n",
			       __func__, (uintmax_t)ac->contract_number);
			break;
		}
		break;
	}
	default:
		break;
	}
}

static cam_status
ctlferegister(struct cam_periph *periph, void *arg)
{
	struct ctlfe_softc *bus_softc;
	struct ctlfe_lun_softc *softc;
	struct cam_sim *sim;
	union ccb en_lun_ccb;
	cam_status status;
	int i;

	softc = (struct ctlfe_lun_softc *)arg;
	bus_softc = softc->parent_softc;
	sim = xpt_path_sim(periph->path);
	
	TAILQ_INIT(&softc->work_queue);
	softc->periph = periph;

	callout_init_mtx(&softc->dma_callout, sim->mtx, /*flags*/ 0);
	periph->softc = softc;

	xpt_setup_ccb(&en_lun_ccb.ccb_h, periph->path, /*priority*/ 1);
	en_lun_ccb.ccb_h.func_code = XPT_EN_LUN;
	en_lun_ccb.cel.grp6_len = 0;
	en_lun_ccb.cel.grp7_len = 0;
	en_lun_ccb.cel.enable = 1;
	xpt_action(&en_lun_ccb);
	status = (en_lun_ccb.ccb_h.status & CAM_STATUS_MASK);
	if (status != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: Enable LUN failed, status 0x%x\n", 
			  __func__, en_lun_ccb.ccb_h.status);
		return (status);
	}

	status = CAM_REQ_CMP;

	for (i = 0; i < CTLFE_ATIO_PER_LUN; i++) {
		union ccb *new_ccb;

		new_ccb = (union ccb *)malloc(sizeof(*new_ccb), M_CTLFE,
					      M_NOWAIT);
		if (new_ccb == NULL) {
			status = CAM_RESRC_UNAVAIL;
			break;
		}
		xpt_setup_ccb(&new_ccb->ccb_h, periph->path, /*priority*/ 1);
		new_ccb->ccb_h.func_code = XPT_ACCEPT_TARGET_IO;
		new_ccb->ccb_h.cbfcnp = ctlfedone;
		xpt_action(new_ccb);
		softc->atios_sent++;
		status = new_ccb->ccb_h.status;
		if ((status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
			free(new_ccb, M_CTLFE);
			break;
		}
	}

	status = cam_periph_acquire(periph);
	if ((status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: could not acquire reference "
			  "count, status = %#x\n", __func__, status);
		return (status);
	}

	if (i == 0) {
		xpt_print(periph->path, "%s: could not allocate ATIO CCBs, "
			  "status 0x%x\n", __func__, status);
		return (CAM_REQ_CMP_ERR);
	}

	for (i = 0; i < CTLFE_IN_PER_LUN; i++) {
		union ccb *new_ccb;

		new_ccb = (union ccb *)malloc(sizeof(*new_ccb), M_CTLFE,
					      M_NOWAIT);
		if (new_ccb == NULL) {
			status = CAM_RESRC_UNAVAIL;
			break;
		}

		xpt_setup_ccb(&new_ccb->ccb_h, periph->path, /*priority*/ 1);
		new_ccb->ccb_h.func_code = XPT_IMMEDIATE_NOTIFY;
		new_ccb->ccb_h.cbfcnp = ctlfedone;
		xpt_action(new_ccb);
		softc->inots_sent++;
		status = new_ccb->ccb_h.status;
		if ((status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
			/*
			 * Note that we don't free the CCB here.  If the
			 * status is not CAM_REQ_INPROG, then we're
			 * probably talking to a SIM that says it is
			 * target-capable but doesn't support the 
			 * XPT_IMMEDIATE_NOTIFY CCB.  i.e. it supports the
			 * older API.  In that case, it'll call xpt_done()
			 * on the CCB, and we need to free it in our done
			 * routine as a result.
			 */
			break;
		}
	}
	if ((i == 0)
	 || (status != CAM_REQ_INPROG)) {
		xpt_print(periph->path, "%s: could not allocate immediate "
			  "notify CCBs, status 0x%x\n", __func__, status);
		return (CAM_REQ_CMP_ERR);
	}
	return (CAM_REQ_CMP);
}

static void
ctlfeoninvalidate(struct cam_periph *periph)
{
	union ccb en_lun_ccb;
	cam_status status;
	struct ctlfe_lun_softc *softc;

	softc = (struct ctlfe_lun_softc *)periph->softc;

	xpt_setup_ccb(&en_lun_ccb.ccb_h, periph->path, /*priority*/ 1);
	en_lun_ccb.ccb_h.func_code = XPT_EN_LUN;
	en_lun_ccb.cel.grp6_len = 0;
	en_lun_ccb.cel.grp7_len = 0;
	en_lun_ccb.cel.enable = 0;
	xpt_action(&en_lun_ccb);
	status = (en_lun_ccb.ccb_h.status & CAM_STATUS_MASK);
	if (status != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: Disable LUN failed, status 0x%x\n",
			  __func__, en_lun_ccb.ccb_h.status);
		/*
		 * XXX KDM what do we do now?
		 */
	}
	xpt_print(periph->path, "LUN removed, %ju ATIOs outstanding, %ju "
		  "INOTs outstanding, %d refs\n", softc->atios_sent -
		  softc->atios_returned, softc->inots_sent -
		  softc->inots_returned, periph->refcount);
}

static void
ctlfecleanup(struct cam_periph *periph)
{
	struct ctlfe_lun_softc *softc;
	struct ctlfe_softc *bus_softc;

	xpt_print(periph->path, "%s: Called\n", __func__);

	softc = (struct ctlfe_lun_softc *)periph->softc;
	bus_softc = softc->parent_softc;

	STAILQ_REMOVE(&bus_softc->lun_softc_list, softc, ctlfe_lun_softc,links);
	
	/*
	 * XXX KDM is there anything else that needs to be done here?
	 */
	free(softc, M_CTLFE);
}

static void
ctlfestart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct ctlfe_lun_softc *softc;
	struct ccb_hdr *ccb_h;

	softc = (struct ctlfe_lun_softc *)periph->softc;

	softc->ccbs_alloced++;

	ccb_h = TAILQ_FIRST(&softc->work_queue);
	if (periph->immediate_priority <= periph->pinfo.priority) {
		panic("shouldn't get to the CCB waiting case!");
		start_ccb->ccb_h.ccb_type = CTLFE_CCB_WAITING;
		SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
				  periph_links.sle);
		periph->immediate_priority = CAM_PRIORITY_NONE;
		wakeup(&periph->ccb_list);
	} else if (ccb_h == NULL) {
		softc->ccbs_freed++;
		xpt_release_ccb(start_ccb);
	} else {
		struct ccb_accept_tio *atio;
		struct ccb_scsiio *csio;
		uint8_t *data_ptr;
		uint32_t dxfer_len;
		ccb_flags flags;
		union ctl_io *io;
		uint8_t scsi_status;

		/* Take the ATIO off the work queue */
		TAILQ_REMOVE(&softc->work_queue, ccb_h, periph_links.tqe);
		atio = (struct ccb_accept_tio *)ccb_h;
		io = (union ctl_io *)ccb_h->io_ptr;
		csio = &start_ccb->csio;

		flags = atio->ccb_h.flags &
			(CAM_DIS_DISCONNECT|CAM_TAG_ACTION_VALID|CAM_DIR_MASK);

		if ((io == NULL)
		 || (io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE) {
			/*
			 * We're done, send status back.
			 */
			flags |= CAM_SEND_STATUS;
			if (io == NULL) {
				scsi_status = SCSI_STATUS_BUSY;
				csio->sense_len = 0;
			} else if ((io->io_hdr.status & CTL_STATUS_MASK) ==
				   CTL_CMD_ABORTED) {
				io->io_hdr.flags &= ~CTL_FLAG_STATUS_QUEUED;

				/*
				 * If this command was aborted, we don't
				 * need to send status back to the SIM.
				 * Just free the CTIO and ctl_io, and
				 * recycle the ATIO back to the SIM.
				 */
				xpt_print(periph->path, "%s: aborted "
					  "command 0x%04x discarded\n",
					  __func__, io->scsiio.tag_num);
				ctl_free_io(io);
				/*
				 * For a wildcard attachment, commands can
				 * come in with a specific target/lun.  Reset
				 * the target and LUN fields back to the
				 * wildcard values before we send them back
				 * down to the SIM.  The SIM has a wildcard
				 * LUN enabled, not whatever target/lun 
				 * these happened to be.
				 */
				if (softc->flags & CTLFE_LUN_WILDCARD) {
					atio->ccb_h.target_id =
						CAM_TARGET_WILDCARD;
					atio->ccb_h.target_lun =
						CAM_LUN_WILDCARD;
				}

				if ((atio->ccb_h.status & CAM_DEV_QFRZN) != 0) {
					cam_release_devq(periph->path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
 							 /*timeout*/0,
							 /*getcount_only*/0);
					atio->ccb_h.status &= ~CAM_DEV_QFRZN;
				}

				ccb_h = TAILQ_FIRST(&softc->work_queue);

				if (atio->ccb_h.func_code != 
				    XPT_ACCEPT_TARGET_IO) {
					xpt_print(periph->path, "%s: func_code "
						  "is %#x\n", __func__,
						  atio->ccb_h.func_code);
				}
				start_ccb->ccb_h.func_code = XPT_ABORT;
				start_ccb->cab.abort_ccb = (union ccb *)atio;
				start_ccb->ccb_h.cbfcnp = ctlfedone;

				/* Tell the SIM that we've aborted this ATIO */
				xpt_action(start_ccb);
				softc->ccbs_freed++;
				xpt_release_ccb(start_ccb);

				/*
				 * Send the ATIO back down to the SIM.
				 */
				xpt_action((union ccb *)atio);
				softc->atios_sent++;

				/*
				 * If we still have work to do, ask for
				 * another CCB.  Otherwise, deactivate our
				 * callout.
				 */
				if (ccb_h != NULL)
					xpt_schedule(periph, /*priority*/ 1);
				else
					callout_stop(&softc->dma_callout);

				return;
			} else {
				io->io_hdr.flags &= ~CTL_FLAG_STATUS_QUEUED;
				scsi_status = io->scsiio.scsi_status;
				csio->sense_len = io->scsiio.sense_len;
			}
			data_ptr = NULL;
			dxfer_len = 0;
			if (io == NULL) {
				printf("%s: tag %04x io is NULL\n", __func__,
				       atio->tag_id);
			} else {
#ifdef CTLFEDEBUG
				printf("%s: tag %04x status %x\n", __func__,
				       atio->tag_id, io->io_hdr.status);
#endif
			}
			csio->sglist_cnt = 0;
			if (csio->sense_len != 0) {
				csio->sense_data = io->scsiio.sense_data;
				flags |= CAM_SEND_SENSE;
			} else if (scsi_status == SCSI_STATUS_CHECK_COND) {
				xpt_print(periph->path, "%s: check condition "
					  "with no sense\n", __func__);
			}
		} else {
			struct ctlfe_lun_cmd_info *cmd_info;

			/*
			 * Datamove call, we need to setup the S/G list. 
			 * If we pass in a S/G list, the isp(4) driver at
			 * least expects physical/bus addresses.
			 */

			cmd_info = (struct ctlfe_lun_cmd_info *)
				io->io_hdr.port_priv;

			KASSERT(sizeof(*cmd_info) < CTL_PORT_PRIV_SIZE,
				("%s: sizeof(struct ctlfe_lun_cmd_info) %zd < "
				"CTL_PORT_PRIV_SIZE %d", __func__,
				sizeof(*cmd_info), CTL_PORT_PRIV_SIZE));
			io->io_hdr.flags &= ~CTL_FLAG_DMA_QUEUED;

			/*
			 * Need to zero this, in case it has been used for
			 * a previous datamove for this particular I/O.
			 */
			bzero(cmd_info, sizeof(*cmd_info));
			scsi_status = 0;

			/*
			 * Set the direction, relative to the initiator.
			 */
			flags &= ~CAM_DIR_MASK;
			if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
			     CTL_FLAG_DATA_IN)
				flags |= CAM_DIR_IN;
			else
				flags |= CAM_DIR_OUT;
			
			csio->cdb_len = atio->cdb_len;

			if (io->scsiio.kern_sg_entries == 0) {
				/* No S/G list */
				data_ptr = io->scsiio.kern_data_ptr;
				dxfer_len = io->scsiio.kern_data_len;
				csio->sglist_cnt = 0;

				if (io->io_hdr.flags & CTL_FLAG_BUS_ADDR)
					flags |= CAM_DATA_PHYS;
			} else if (io->scsiio.kern_sg_entries <=
				   (sizeof(cmd_info->cam_sglist)/
				   sizeof(cmd_info->cam_sglist[0]))) {
				/*
				 * S/G list with physical or virtual pointers.
				 * Just populate the CAM S/G list with the
				 * pointers.
				 */
				int i;
				struct ctl_sg_entry *ctl_sglist;
				bus_dma_segment_t *cam_sglist;

				ctl_sglist = (struct ctl_sg_entry *)
					io->scsiio.kern_data_ptr;
				cam_sglist = cmd_info->cam_sglist;

				for (i = 0; i < io->scsiio.kern_sg_entries;i++){
					cam_sglist[i].ds_addr =
						(bus_addr_t)ctl_sglist[i].addr;
					cam_sglist[i].ds_len =
						ctl_sglist[i].len;
				}
				csio->sglist_cnt = io->scsiio.kern_sg_entries;
				flags |= CAM_SCATTER_VALID;
				if (io->io_hdr.flags & CTL_FLAG_BUS_ADDR)
					flags |= CAM_SG_LIST_PHYS;
				else
					flags &= ~CAM_SG_LIST_PHYS;
				data_ptr = (uint8_t *)cam_sglist;
				dxfer_len = io->scsiio.kern_data_len;
			} else {
				/* S/G list with virtual pointers */
				struct ctl_sg_entry *sglist;
				int *ti;

				/*
				 * XXX KDM this is a temporary hack.  The
				 * isp(4) driver can't deal with S/G lists
				 * with virtual pointers, so we need to
				 * go through and send down one virtual
				 * pointer at a time.
				 */
				sglist = (struct ctl_sg_entry *)
					io->scsiio.kern_data_ptr;
				ti = &cmd_info->cur_transfer_index;
				data_ptr = sglist[*ti].addr;
				dxfer_len = sglist[*ti].len;
				csio->sglist_cnt = 0;
				cmd_info->flags |= CTLFE_CMD_PIECEWISE;
				(*ti)++;
			}

			io->scsiio.ext_data_filled += dxfer_len;

			if (io->scsiio.ext_data_filled >
			    io->scsiio.kern_total_len) {
				xpt_print(periph->path, "%s: tag 0x%04x "
					  "fill len %u > total %u\n",
					  __func__, io->scsiio.tag_num,
					  io->scsiio.ext_data_filled,
					  io->scsiio.kern_total_len);
			}
		}

#ifdef CTLFEDEBUG
		printf("%s: %s: tag %04x flags %x ptr %p len %u\n", __func__,
		       (flags & CAM_SEND_STATUS) ? "done" : "datamove",
		       atio->tag_id, flags, data_ptr, dxfer_len);
#endif

		/*
		 * Valid combinations:
		 *  - CAM_SEND_STATUS, SCATTER_VALID = 0, dxfer_len = 0,
		 *    sglist_cnt = 0
		 *  - CAM_SEND_STATUS = 0, SCATTER_VALID = 0, dxfer_len != 0,
		 *    sglist_cnt = 0 
		 *  - CAM_SEND_STATUS = 0, SCATTER_VALID, dxfer_len != 0,
		 *    sglist_cnt != 0
		 */
#ifdef CTLFEDEBUG
		if (((flags & CAM_SEND_STATUS)
		  && (((flags & CAM_SCATTER_VALID) != 0)
		   || (dxfer_len != 0)
		   || (csio->sglist_cnt != 0)))
		 || (((flags & CAM_SEND_STATUS) == 0)
		  && (dxfer_len == 0))
		 || ((flags & CAM_SCATTER_VALID)
		  && (csio->sglist_cnt == 0))
		 || (((flags & CAM_SCATTER_VALID) == 0)
		  && (csio->sglist_cnt != 0))) {
			printf("%s: tag %04x cdb %02x flags %#x dxfer_len "
			       "%d sg %u\n", __func__, atio->tag_id,
			       atio->cdb_io.cdb_bytes[0], flags, dxfer_len,
			       csio->sglist_cnt);
			if (io != NULL) {
				printf("%s: tag %04x io status %#x\n", __func__,
				       atio->tag_id, io->io_hdr.status);
			} else {
				printf("%s: tag %04x no associated io\n",
				       __func__, atio->tag_id);
			}
		}
#endif
		cam_fill_ctio(csio,
			      /*retries*/ 2,
			      ctlfedone,
			      flags,
			      (flags & CAM_TAG_ACTION_VALID) ?
			       MSG_SIMPLE_Q_TAG : 0,
			      atio->tag_id,
			      atio->init_id,
			      scsi_status,
			      /*data_ptr*/ data_ptr,
			      /*dxfer_len*/ dxfer_len,
			      /*timeout*/ 5 * 1000);
		start_ccb->ccb_h.ccb_atio = atio;
		if (((flags & CAM_SEND_STATUS) == 0)
		 && (io != NULL))
			io->io_hdr.flags |= CTL_FLAG_DMA_INPROG;

		softc->ctios_sent++;

		xpt_action(start_ccb);

		if ((atio->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			cam_release_devq(periph->path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
 					 /*timeout*/0,
					 /*getcount_only*/0);
			atio->ccb_h.status &= ~CAM_DEV_QFRZN;
		}

		ccb_h = TAILQ_FIRST(&softc->work_queue);
	}
	/*
	 * If we still have work to do, ask for another CCB.  Otherwise,
	 * deactivate our callout.
	 */
	if (ccb_h != NULL)
		xpt_schedule(periph, /*priority*/ 1);
	else
		callout_stop(&softc->dma_callout);
}

static void
ctlfe_free_ccb(struct cam_periph *periph, union ccb *ccb)
{
	struct ctlfe_lun_softc *softc;

	softc = (struct ctlfe_lun_softc *)periph->softc;

	switch (ccb->ccb_h.func_code) {
	case XPT_ACCEPT_TARGET_IO:
		softc->atios_returned++;
		break;
	case XPT_IMMEDIATE_NOTIFY:
	case XPT_NOTIFY_ACKNOWLEDGE:
		softc->inots_returned++;
		break;
	default:
		break;
	}

	free(ccb, M_CTLFE);

	KASSERT(softc->atios_returned <= softc->atios_sent, ("%s: "
		"atios_returned %ju > atios_sent %ju", __func__,
		softc->atios_returned, softc->atios_sent));
	KASSERT(softc->inots_returned <= softc->inots_sent, ("%s: "
		"inots_returned %ju > inots_sent %ju", __func__,
		softc->inots_returned, softc->inots_sent));

	/*
	 * If we have received all of our CCBs, we can release our
	 * reference on the peripheral driver.  It will probably go away
	 * now.
	 */
	if ((softc->atios_returned == softc->atios_sent)
	 && (softc->inots_returned == softc->inots_sent)) {
		cam_periph_release_locked(periph);
	}
}

static void
ctlfedone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct ctlfe_lun_softc *softc;
	struct ctlfe_softc *bus_softc;

#ifdef CTLFE_DEBUG
	printf("%s: entered, func_code = %#x, type = %#lx\n", __func__,
	       done_ccb->ccb_h.func_code, done_ccb->ccb_h.ccb_type);
#endif

	softc = (struct ctlfe_lun_softc *)periph->softc;
	bus_softc = softc->parent_softc;

	if (done_ccb->ccb_h.ccb_type == CTLFE_CCB_WAITING) {
		panic("shouldn't get to the CCB waiting case!");
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}

	/*
	 * If the peripheral is invalid, ATIOs and immediate notify CCBs
	 * need to be freed.  Most of the ATIOs and INOTs that come back
	 * will be CCBs that are being returned from the SIM as a result of
	 * our disabling the LUN.
	 *
	 * Other CCB types are handled in their respective cases below.
	 */
	if (periph->flags & CAM_PERIPH_INVALID) {
		switch (done_ccb->ccb_h.func_code) {
		case XPT_ACCEPT_TARGET_IO:
		case XPT_IMMEDIATE_NOTIFY:
		case XPT_NOTIFY_ACKNOWLEDGE:
			ctlfe_free_ccb(periph, done_ccb);
			return;
		default:
			break;
		}

	}
	switch (done_ccb->ccb_h.func_code) {
	case XPT_ACCEPT_TARGET_IO: {
		union ctl_io *io;
		struct ccb_accept_tio *atio;

		atio = &done_ccb->atio;

		softc->atios_returned++;

		/*
		 * Allocate a ctl_io, pass it to CTL, and wait for the
		 * datamove or done.
		 */
		io = ctl_alloc_io(bus_softc->fe.ctl_pool_ref);
		if (io == NULL) {
			atio->ccb_h.flags &= ~CAM_DIR_MASK;
			atio->ccb_h.flags |= CAM_DIR_NONE;

			printf("%s: ctl_alloc_io failed!\n", __func__);

			/*
			 * XXX KDM need to set SCSI_STATUS_BUSY, but there
			 * is no field in the ATIO structure to do that,
			 * and we aren't able to allocate a ctl_io here.
			 * What to do?
			 */
			atio->sense_len = 0;
			done_ccb->ccb_h.io_ptr = NULL;
			TAILQ_INSERT_TAIL(&softc->work_queue, &atio->ccb_h,
					  periph_links.tqe);
			xpt_schedule(periph, /*priority*/ 1);
			break;
		}
		ctl_zero_io(io);

		/* Save pointers on both sides */
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = done_ccb;
		done_ccb->ccb_h.io_ptr = io;

		/*
		 * Only SCSI I/O comes down this path, resets, etc. come
		 * down the immediate notify path below.
		 */
		io->io_hdr.io_type = CTL_IO_SCSI;
		io->io_hdr.nexus.initid.id = atio->init_id;
		io->io_hdr.nexus.targ_port = bus_softc->fe.targ_port;
		io->io_hdr.nexus.targ_target.id = atio->ccb_h.target_id;
		io->io_hdr.nexus.targ_lun = atio->ccb_h.target_lun;
		io->scsiio.tag_num = atio->tag_id;
		switch (atio->tag_action) {
		case CAM_TAG_ACTION_NONE:
			io->scsiio.tag_type = CTL_TAG_UNTAGGED;
			break;
		case MSG_SIMPLE_TASK:
			io->scsiio.tag_type = CTL_TAG_SIMPLE;
			break;
		case MSG_HEAD_OF_QUEUE_TASK:
        		io->scsiio.tag_type = CTL_TAG_HEAD_OF_QUEUE;
			break;
		case MSG_ORDERED_TASK:
        		io->scsiio.tag_type = CTL_TAG_ORDERED;
			break;
		case MSG_ACA_TASK:
			io->scsiio.tag_type = CTL_TAG_ACA;
			break;
		default:
			io->scsiio.tag_type = CTL_TAG_UNTAGGED;
			printf("%s: unhandled tag type %#x!!\n", __func__,
			       atio->tag_action);
			break;
		}
		if (atio->cdb_len > sizeof(io->scsiio.cdb)) {
			printf("%s: WARNING: CDB len %d > ctl_io space %zd\n",
			       __func__, atio->cdb_len, sizeof(io->scsiio.cdb));
		}
		io->scsiio.cdb_len = min(atio->cdb_len, sizeof(io->scsiio.cdb));
		bcopy(atio->cdb_io.cdb_bytes, io->scsiio.cdb,
		      io->scsiio.cdb_len);

#ifdef CTLFEDEBUG
		printf("%s: %ju:%d:%ju:%d: tag %04x CDB %02x\n", __func__,
		        (uintmax_t)io->io_hdr.nexus.initid.id,
		        io->io_hdr.nexus.targ_port,
		        (uintmax_t)io->io_hdr.nexus.targ_target.id,
		        io->io_hdr.nexus.targ_lun,
			io->scsiio.tag_num, io->scsiio.cdb[0]);
#endif

		ctl_queue(io);
		break;
	}
	case XPT_CONT_TARGET_IO: {
		struct ccb_accept_tio *atio;
		union ctl_io *io;

		atio = (struct ccb_accept_tio *)done_ccb->ccb_h.ccb_atio;
		io = (union ctl_io *)atio->ccb_h.io_ptr;

		softc->ctios_returned++;
#ifdef CTLFEDEBUG
		printf("%s: got XPT_CONT_TARGET_IO tag %#x flags %#x\n",
		       __func__, atio->tag_id, done_ccb->ccb_h.flags);
#endif
		/*
		 * If we were sending status back to the initiator, free up
		 * resources.  If we were doing a datamove, call the
		 * datamove done routine.
		 */
		if (done_ccb->ccb_h.flags & CAM_SEND_STATUS) {
			softc->ccbs_freed++;
			xpt_release_ccb(done_ccb);
			ctl_free_io(io);
			/*
			 * For a wildcard attachment, commands can come in
			 * with a specific target/lun.  Reset the target
			 * and LUN fields back to the wildcard values before
			 * we send them back down to the SIM.  The SIM has
			 * a wildcard LUN enabled, not whatever target/lun
			 * these happened to be.
			 */
			if (softc->flags & CTLFE_LUN_WILDCARD) {
				atio->ccb_h.target_id = CAM_TARGET_WILDCARD;
				atio->ccb_h.target_lun = CAM_LUN_WILDCARD;
			}
			if (periph->flags & CAM_PERIPH_INVALID) {
				ctlfe_free_ccb(periph, (union ccb *)atio);
				return;
			} else {
				xpt_action((union ccb *)atio);
				softc->atios_sent++;
			}
		} else {
			struct ctlfe_lun_cmd_info *cmd_info;
			struct ccb_scsiio *csio;

			csio = &done_ccb->csio;
			cmd_info = (struct ctlfe_lun_cmd_info *)
				io->io_hdr.port_priv;

			io->io_hdr.flags &= ~CTL_FLAG_DMA_INPROG;

			io->scsiio.ext_data_len += csio->dxfer_len;
			if (io->scsiio.ext_data_len >
			    io->scsiio.kern_total_len) {
				xpt_print(periph->path, "%s: tag 0x%04x "
					  "done len %u > total %u sent %u\n",
					  __func__, io->scsiio.tag_num,
					  io->scsiio.ext_data_len,
					  io->scsiio.kern_total_len,
					  io->scsiio.ext_data_filled);
			}
			/*
			 * Translate CAM status to CTL status.  Success
			 * does not change the overall, ctl_io status.  In
			 * that case we just set port_status to 0.  If we
			 * have a failure, though, set a data phase error
			 * for the overall ctl_io.
			 */
			switch (done_ccb->ccb_h.status & CAM_STATUS_MASK) {
			case CAM_REQ_CMP:
				io->io_hdr.port_status = 0;
				break;
			default:
				/*
				 * XXX KDM the isp(4) driver doesn't really
				 * seem to send errors back for data
				 * transfers that I can tell.  There is one
				 * case where it'll send CAM_REQ_CMP_ERR,
				 * but probably not that many more cases.
				 * So set a generic data phase error here,
				 * like the SXP driver sets.
				 */
				io->io_hdr.port_status = 0xbad1;
				ctl_set_data_phase_error(&io->scsiio);
				/*
				 * XXX KDM figure out residual.
				 */
				break;
			}
			/*
			 * If we had to break this S/G list into multiple
			 * pieces, figure out where we are in the list, and
			 * continue sending pieces if necessary.
			 */
			if ((cmd_info->flags & CTLFE_CMD_PIECEWISE)
			 && (io->io_hdr.port_status == 0)
			 && (cmd_info->cur_transfer_index <
			     io->scsiio.kern_sg_entries)) {
				struct ctl_sg_entry *sglist;
				ccb_flags flags;
				uint8_t scsi_status;
				uint8_t *data_ptr;
				uint32_t dxfer_len;
				int *ti;

				sglist = (struct ctl_sg_entry *)
					io->scsiio.kern_data_ptr;
				ti = &cmd_info->cur_transfer_index;
				flags = atio->ccb_h.flags &
					(CAM_DIS_DISCONNECT|
					 CAM_TAG_ACTION_VALID|
					 CAM_DIR_MASK);
				
				/*
				 * Set the direction, relative to the initiator.
				 */
				flags &= ~CAM_DIR_MASK;
				if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
				     CTL_FLAG_DATA_IN)
					flags |= CAM_DIR_IN;
				else
					flags |= CAM_DIR_OUT;

				data_ptr = sglist[*ti].addr;
				dxfer_len = sglist[*ti].len;
				(*ti)++;

				scsi_status = 0;

				if (((flags & CAM_SEND_STATUS) == 0)
				 && (dxfer_len == 0)) {
					printf("%s: tag %04x no status or "
					       "len cdb = %02x\n", __func__,
					       atio->tag_id,
					atio->cdb_io.cdb_bytes[0]);
					printf("%s: tag %04x io status %#x\n",
					       __func__, atio->tag_id,
					       io->io_hdr.status);
				}

				cam_fill_ctio(csio,
					      /*retries*/ 2,
					      ctlfedone,
					      flags,
					      (flags & CAM_TAG_ACTION_VALID) ?
					       MSG_SIMPLE_Q_TAG : 0,
					      atio->tag_id,
					      atio->init_id,
					      scsi_status,
					      /*data_ptr*/ data_ptr,
					      /*dxfer_len*/ dxfer_len,
					      /*timeout*/ 5 * 1000);

				csio->resid = 0;
				csio->ccb_h.ccb_atio = atio;
				io->io_hdr.flags |= CTL_FLAG_DMA_INPROG;
				softc->ctios_sent++;
				xpt_action((union ccb *)csio);
			} else {
				/*
				 * Release the CTIO.  The ATIO will be sent back
				 * down to the SIM once we send status.
				 */
				softc->ccbs_freed++;
				xpt_release_ccb(done_ccb);

				/* Call the backend move done callback */
				io->scsiio.be_move_done(io);
			}
		}
		break;
	}
	case XPT_IMMEDIATE_NOTIFY: {
		union ctl_io *io;
		struct ccb_immediate_notify *inot;
		cam_status status;
		int frozen;

		inot = &done_ccb->cin1;

		softc->inots_returned++;

		frozen = (done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0;

		printf("%s: got XPT_IMMEDIATE_NOTIFY status %#x tag %#x "
		       "seq %#x\n", __func__, inot->ccb_h.status,
		       inot->tag_id, inot->seq_id);

		io = ctl_alloc_io(bus_softc->fe.ctl_pool_ref);
		if (io != NULL) {
			int send_ctl_io;

			send_ctl_io = 1;

			ctl_zero_io(io);		
			io->io_hdr.io_type = CTL_IO_TASK;
			io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr =done_ccb;
			inot->ccb_h.io_ptr = io;
			io->io_hdr.nexus.initid.id = inot->initiator_id;
			io->io_hdr.nexus.targ_port = bus_softc->fe.targ_port;
			io->io_hdr.nexus.targ_target.id = inot->ccb_h.target_id;
			io->io_hdr.nexus.targ_lun = inot->ccb_h.target_lun;
			/* XXX KDM should this be the tag_id? */
			io->taskio.tag_num = inot->seq_id;

			status = inot->ccb_h.status & CAM_STATUS_MASK;
			switch (status) {
			case CAM_SCSI_BUS_RESET:
				io->taskio.task_action = CTL_TASK_BUS_RESET;
				break;
			case CAM_BDR_SENT:
				io->taskio.task_action = CTL_TASK_TARGET_RESET;
				break;
			case CAM_MESSAGE_RECV:
				switch (inot->arg) {
				case MSG_ABORT_TASK_SET:
					/*
					 * XXX KDM this isn't currently
					 * supported by CTL.  It ends up
					 * being a no-op.
					 */
					io->taskio.task_action =
						CTL_TASK_ABORT_TASK_SET;
					break;
				case MSG_TARGET_RESET:
					io->taskio.task_action =
						CTL_TASK_TARGET_RESET;
					break;
				case MSG_ABORT_TASK:
					io->taskio.task_action =
						CTL_TASK_ABORT_TASK;
					break;
				case MSG_LOGICAL_UNIT_RESET:
					io->taskio.task_action =
						CTL_TASK_LUN_RESET;
					break;
				case MSG_CLEAR_TASK_SET:
					/*
					 * XXX KDM this isn't currently
					 * supported by CTL.  It ends up
					 * being a no-op.
					 */
					io->taskio.task_action =
						CTL_TASK_CLEAR_TASK_SET;
					break;
				case MSG_CLEAR_ACA:
					io->taskio.task_action = 
						CTL_TASK_CLEAR_ACA;
					break;
				case MSG_NOOP:
					send_ctl_io = 0;
					break;
				default:
					xpt_print(periph->path, "%s: "
						  "unsupported message 0x%x\n", 
						  __func__, inot->arg);
					send_ctl_io = 0;
					break;
				}
				break;
			case CAM_REQ_ABORTED:
				/*
				 * This request was sent back by the driver.
				 * XXX KDM what do we do here?
				 */
				send_ctl_io = 0;
				break;
			case CAM_REQ_INVALID:
			case CAM_PROVIDE_FAIL:
			default:
				/*
				 * We should only get here if we're talking
				 * to a talking to a SIM that is target
				 * capable but supports the old API.  In
				 * that case, we need to just free the CCB.
				 * If we actually send a notify acknowledge,
				 * it will send that back with an error as
				 * well.
				 */

				if ((status != CAM_REQ_INVALID)
				 && (status != CAM_PROVIDE_FAIL))
					xpt_print(periph->path, "%s: "
						  "unsupported CAM status "
						  "0x%x\n", __func__, status);

				ctl_free_io(io);
				ctlfe_free_ccb(periph, done_ccb);

				return;
			}
			if (send_ctl_io != 0) {
				ctl_queue(io);
			} else {
				ctl_free_io(io);
				done_ccb->ccb_h.status = CAM_REQ_INPROG;
				done_ccb->ccb_h.func_code =
					XPT_NOTIFY_ACKNOWLEDGE;
				xpt_action(done_ccb);
			}
		} else {
			xpt_print(periph->path, "%s: could not allocate "
				  "ctl_io for immediate notify!\n", __func__);
			/* requeue this to the adapter */
			done_ccb->ccb_h.status = CAM_REQ_INPROG;
			done_ccb->ccb_h.func_code = XPT_NOTIFY_ACKNOWLEDGE;
			xpt_action(done_ccb);
		}

		if (frozen != 0) {
			cam_release_devq(periph->path,
					 /*relsim_flags*/ 0,
					 /*opening reduction*/ 0,
					 /*timeout*/ 0,
					 /*getcount_only*/ 0);
		}
		break;
	}
	case XPT_NOTIFY_ACKNOWLEDGE:
		/*
		 * Queue this back down to the SIM as an immediate notify.
		 */
		done_ccb->ccb_h.func_code = XPT_IMMEDIATE_NOTIFY;
		xpt_action(done_ccb);
		softc->inots_sent++;
		break;
	case XPT_ABORT:
		/*
		 * XPT_ABORT is an immediate CCB, we shouldn't get here.
		 */
		panic("%s: XPT_ABORT CCB returned!", __func__);
		break;
	case XPT_SET_SIM_KNOB:
	case XPT_GET_SIM_KNOB:
		break;
	default:
		panic("%s: unexpected CCB type %#x", __func__,
		      done_ccb->ccb_h.func_code);
		break;
	}
}

static void
ctlfe_onoffline(void *arg, int online)
{
	struct ctlfe_softc *bus_softc;
	union ccb *ccb;
	cam_status status;
	struct cam_path *path;
	struct cam_sim *sim;
	int set_wwnn;

	bus_softc = (struct ctlfe_softc *)arg;

	set_wwnn = 0;

	status = xpt_create_path(&path, /*periph*/ NULL, bus_softc->path_id,
		CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
	if (status != CAM_REQ_CMP) {
		printf("%s: unable to create path!\n", __func__);
		return;
	}
	ccb = (union ccb *)malloc(sizeof(*ccb), M_TEMP, M_WAITOK | M_ZERO);
	if (ccb == NULL) {
		printf("%s: unable to malloc CCB!\n", __func__);
		xpt_free_path(path);
		return;
	}
	xpt_setup_ccb(&ccb->ccb_h, path, /*priority*/ 1);

	sim = xpt_path_sim(path);

	/*
	 * Copan WWN format:
	 *
	 * Bits 63-60:	0x5		NAA, IEEE registered name
	 * Bits 59-36:	0x000ED5	IEEE Company name assigned to Copan
	 * Bits 35-12:			Copan SSN (Sequential Serial Number)
	 * Bits 11-8:			Type of port:
	 *					1 == N-Port
	 *					2 == F-Port
	 *					3 == NL-Port
	 * Bits 7-0:			0 == Node Name, >0 == Port Number
	 */

	if (online != 0) {

		ccb->ccb_h.func_code = XPT_GET_SIM_KNOB;

		CAM_SIM_LOCK(sim);

		xpt_action(ccb);

		CAM_SIM_UNLOCK(sim);

		if ((ccb->knob.xport_specific.valid & KNOB_VALID_ADDRESS) != 0){
#ifdef RANDOM_WWNN
			uint64_t random_bits;
#endif

			printf("%s: %s current WWNN %#jx\n", __func__,
			       bus_softc->port_name,
			       ccb->knob.xport_specific.fc.wwnn);
			printf("%s: %s current WWPN %#jx\n", __func__,
			       bus_softc->port_name,
			       ccb->knob.xport_specific.fc.wwpn);

#ifdef RANDOM_WWNN
			arc4rand(&random_bits, sizeof(random_bits), 0);
#endif

			/*
			 * XXX KDM this is a bit of a kludge for now.  We
			 * take the current WWNN/WWPN from the card, and
			 * replace the company identifier and the NL-Port
			 * indicator and the port number (for the WWPN).
			 * This should be replaced later with ddb_GetWWNN,
			 * or possibly a more centralized scheme.  (It
			 * would be nice to have the WWNN/WWPN for each
			 * port stored in the ctl_frontend structure.)
			 */
#ifdef RANDOM_WWNN
			ccb->knob.xport_specific.fc.wwnn = 
				(random_bits &
				0x0000000fffffff00ULL) |
				/* Company ID */ 0x5000ED5000000000ULL |
				/* NL-Port */    0x0300;
			ccb->knob.xport_specific.fc.wwpn = 
				(random_bits &
				0x0000000fffffff00ULL) |
				/* Company ID */ 0x5000ED5000000000ULL |
				/* NL-Port */    0x3000 |
				/* Port Num */ (bus_softc->fe.targ_port & 0xff);

			/*
			 * This is a bit of an API break/reversal, but if
			 * we're doing the random WWNN that's a little
			 * different anyway.  So record what we're actually
			 * using with the frontend code so it's reported
			 * accurately.
			 */
			bus_softc->fe.wwnn = 
				ccb->knob.xport_specific.fc.wwnn;
			bus_softc->fe.wwpn = 
				ccb->knob.xport_specific.fc.wwpn;
			set_wwnn = 1;
#else /* RANDOM_WWNN */
			/*
			 * If the user has specified a WWNN/WWPN, send them
			 * down to the SIM.  Otherwise, record what the SIM
			 * has reported.
			 */
			if ((bus_softc->fe.wwnn != 0)
			 && (bus_softc->fe.wwpn != 0)) {
				ccb->knob.xport_specific.fc.wwnn =
					bus_softc->fe.wwnn;
				ccb->knob.xport_specific.fc.wwpn =
					bus_softc->fe.wwpn;
				set_wwnn = 1;
			} else {
				bus_softc->fe.wwnn =
					ccb->knob.xport_specific.fc.wwnn;
				bus_softc->fe.wwpn =
					ccb->knob.xport_specific.fc.wwpn;
			}
#endif /* RANDOM_WWNN */


			if (set_wwnn != 0) {
				printf("%s: %s new WWNN %#jx\n", __func__,
				       bus_softc->port_name,
				ccb->knob.xport_specific.fc.wwnn);
				printf("%s: %s new WWPN %#jx\n", __func__,
				       bus_softc->port_name,
				       ccb->knob.xport_specific.fc.wwpn);
			}
		} else {
			printf("%s: %s has no valid WWNN/WWPN\n", __func__,
			       bus_softc->port_name);
		}
	}
	ccb->ccb_h.func_code = XPT_SET_SIM_KNOB;
	ccb->knob.xport_specific.valid = KNOB_VALID_ROLE;
	if (set_wwnn != 0)
		ccb->knob.xport_specific.valid |= KNOB_VALID_ADDRESS;

	if (online != 0)
		ccb->knob.xport_specific.fc.role = KNOB_ROLE_TARGET;
	else
		ccb->knob.xport_specific.fc.role = KNOB_ROLE_NONE;


	CAM_SIM_LOCK(sim);

	xpt_action(ccb);

	CAM_SIM_UNLOCK(sim);

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		printf("%s: SIM %s (path id %d) target %s failed with "
		       "status %#x\n",
		       __func__, bus_softc->port_name, bus_softc->path_id,
		       (online != 0) ? "enable" : "disable",
		       ccb->ccb_h.status);
	} else {
		printf("%s: SIM %s (path id %d) target %s succeeded\n",
		       __func__, bus_softc->port_name, bus_softc->path_id,
		       (online != 0) ? "enable" : "disable");
	}

	free(ccb, M_TEMP);
	xpt_free_path(path);

	return;
}

static void
ctlfe_online(void *arg)
{
	ctlfe_onoffline(arg, /*online*/ 1);
}

static void
ctlfe_offline(void *arg)
{
	ctlfe_onoffline(arg, /*online*/ 0);
}

static int
ctlfe_targ_enable(void *arg, struct ctl_id targ_id)
{
	return (0);
}

static int
ctlfe_targ_disable(void *arg, struct ctl_id targ_id)
{
	return (0);
}

/*
 * This will get called to enable a LUN on every bus that is attached to
 * CTL.  So we only need to create a path/periph for this particular bus.
 */
static int
ctlfe_lun_enable(void *arg, struct ctl_id targ_id, int lun_id)
{
	struct ctlfe_softc *bus_softc;
	struct ctlfe_lun_softc *softc;
	struct cam_path *path;
	struct cam_periph *periph;
	struct cam_sim *sim;
	cam_status status;

	
	bus_softc = (struct ctlfe_softc *)arg;

	status = xpt_create_path_unlocked(&path, /*periph*/ NULL,
					  bus_softc->path_id,
					  targ_id.id,
					  lun_id);
	/* XXX KDM need some way to return status to CTL here? */
	if (status != CAM_REQ_CMP) {
		printf("%s: could not create path, status %#x\n", __func__,
		       status);
		return (1);
	}

	softc = malloc(sizeof(*softc), M_CTLFE, M_WAITOK | M_ZERO);
	if (softc == NULL) {
		printf("%s: could not allocate %zd bytes for softc\n",
		       __func__, sizeof(*softc));
		xpt_free_path(path);
		return (1);
	}
	sim = xpt_path_sim(path);
	mtx_lock(sim->mtx);
	periph = cam_periph_find(path, "ctl");
	if (periph != NULL) {
		/* We've already got a periph, no need to alloc a new one. */
		xpt_free_path(path);
		free(softc, M_CTLFE);
		mtx_unlock(sim->mtx);
		return (0);
	}

	softc->parent_softc = bus_softc;
	STAILQ_INSERT_TAIL(&bus_softc->lun_softc_list, softc, links);

	status = cam_periph_alloc(ctlferegister,
				  ctlfeoninvalidate,
				  ctlfecleanup,
				  ctlfestart,
				  "ctl",
				  CAM_PERIPH_BIO,
				  path,
				  ctlfeasync,
				  0,
				  softc);

	mtx_unlock(sim->mtx);

	xpt_free_path(path);

	return (0);
}

/*
 * XXX KDM we disable LUN removal here.  The problem is that the isp(4)
 * driver doesn't currently handle LUN removal properly.  We need to keep 
 * enough state here at the peripheral level even after LUNs have been
 * removed inside CTL.
 *
 * Once the isp(4) driver is fixed, this can be re-enabled.
 */
static int
ctlfe_lun_disable(void *arg, struct ctl_id targ_id, int lun_id)
{
#ifdef NOTYET
	struct ctlfe_softc *softc;
	struct ctlfe_lun_softc *lun_softc;

	softc = (struct ctlfe_softc *)arg;

	mtx_lock(softc->sim->mtx);
	STAILQ_FOREACH(lun_softc, &softc->lun_softc_list, links) {
		struct cam_path *path;

		path = lun_softc->periph->path;

		if ((xpt_path_target_id(path) == targ_id.id)
		 && (xpt_path_lun_id(path) == lun_id)) {
			break;
		}
	}
	if (lun_softc == NULL) {
		mtx_unlock(softc->sim->mtx);
		printf("%s: can't find target %d lun %d\n", __func__,
		       targ_id.id, lun_id);
		return (1);
	}

	cam_periph_invalidate(lun_softc->periph);

	mtx_unlock(softc->sim->mtx);
#endif

	return (0);
}

static void
ctlfe_dump_sim(struct cam_sim *sim)
{
	int i;

	printf("%s%d: max tagged openings: %d, max dev openings: %d\n",
	       sim->sim_name, sim->unit_number,
	       sim->max_tagged_dev_openings, sim->max_dev_openings);
	printf("%s%d: max_ccbs: %u, ccb_count: %u\n", 
	       sim->sim_name, sim->unit_number,
	       sim->max_ccbs, sim->ccb_count);
	printf("%s%d: ccb_freeq is %sempty\n",
	       sim->sim_name, sim->unit_number,
	       (SLIST_FIRST(&sim->ccb_freeq) == NULL) ? "" : "NOT ");
	printf("%s%d: alloc_queue.entries %d, alloc_openings %d\n",
	       sim->sim_name, sim->unit_number,
	       sim->devq->alloc_queue.entries, sim->devq->alloc_openings);
	printf("%s%d: qfrozen_cnt:", sim->sim_name, sim->unit_number);
	for (i = 0; i < CAM_RL_VALUES; i++) {
		printf("%s%u", (i != 0) ? ":" : "",
		sim->devq->alloc_queue.qfrozen_cnt[i]);
	}
	printf("\n");
}

/*
 * Assumes that the SIM lock is held.
 */
static void
ctlfe_dump_queue(struct ctlfe_lun_softc *softc)
{
	struct ccb_hdr *hdr;
	struct cam_periph *periph;
	int num_items;

	periph = softc->periph;
	num_items = 0;

	TAILQ_FOREACH(hdr, &softc->work_queue, periph_links.tqe) {
		union ctl_io *io;

		io = hdr->io_ptr;

		num_items++;

		/*
		 * This can happen when we get an ATIO but can't allocate
		 * a ctl_io.  See the XPT_ACCEPT_TARGET_IO case in ctlfedone().
		 */
		if (io == NULL) {
			struct ccb_scsiio *csio;

			csio = (struct ccb_scsiio *)hdr;

			xpt_print(periph->path, "CCB %#x ctl_io allocation "
				  "failed\n", csio->tag_id);
			continue;
		}

		/*
		 * Only regular SCSI I/O is put on the work
		 * queue, so we can print sense here.  There may be no
		 * sense if it's no the queue for a DMA, but this serves to
		 * print out the CCB as well.
		 *
		 * XXX KDM switch this over to scsi_sense_print() when
		 * CTL is merged in with CAM.
		 */
		ctl_io_error_print(io, NULL);

		/*
		 * We're sending status back to the
		 * initiator, so we're on the queue waiting
		 * for a CTIO to do that.
		 */
		if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE)
			continue;

		/*
		 * Otherwise, we're on the queue waiting to
		 * do a data transfer.
		 */
		xpt_print(periph->path, "Total %u, Current %u, Resid %u\n",
			  io->scsiio.kern_total_len, io->scsiio.kern_data_len,
			  io->scsiio.kern_data_resid);
	}

	xpt_print(periph->path, "%d requests total waiting for CCBs\n",
		  num_items);
	xpt_print(periph->path, "%ju CCBs oustanding (%ju allocated, %ju "
		  "freed)\n", (uintmax_t)(softc->ccbs_alloced -
		  softc->ccbs_freed), (uintmax_t)softc->ccbs_alloced,
		  (uintmax_t)softc->ccbs_freed);
	xpt_print(periph->path, "%ju CTIOs outstanding (%ju sent, %ju "
		  "returned\n", (uintmax_t)(softc->ctios_sent -
		  softc->ctios_returned), softc->ctios_sent,
		  softc->ctios_returned);
}

/*
 * This function is called when we fail to get a CCB for a DMA or status return
 * to the initiator within the specified time period.
 *
 * The callout code should insure that we hold the sim mutex here.
 */
static void
ctlfe_dma_timeout(void *arg)
{
	struct ctlfe_lun_softc *softc;
	struct cam_periph *periph;
	struct cam_sim *sim;
	int num_queued;

	softc = (struct ctlfe_lun_softc *)arg;
	periph = softc->periph;
	sim = xpt_path_sim(periph->path);
	num_queued = 0;

	/*
	 * Nothing to do...
	 */
	if (TAILQ_FIRST(&softc->work_queue) == NULL) {
		xpt_print(periph->path, "TIMEOUT triggered after %d "
			  "seconds, but nothing on work queue??\n",
			  CTLFE_DMA_TIMEOUT);
		return;
	}

	xpt_print(periph->path, "TIMEOUT (%d seconds) waiting for DMA to "
		  "start\n", CTLFE_DMA_TIMEOUT);

	ctlfe_dump_queue(softc);

	ctlfe_dump_sim(sim);

	xpt_print(periph->path, "calling xpt_schedule() to attempt to "
		  "unstick our queue\n");

	xpt_schedule(periph, /*priority*/ 1);

	xpt_print(periph->path, "xpt_schedule() call complete\n");
}

/*
 * Datamove/done routine called by CTL.  Put ourselves on the queue to
 * receive a CCB from CAM so we can queue the continue I/O request down
 * to the adapter.
 */
static void
ctlfe_datamove_done(union ctl_io *io)
{
	union ccb *ccb;
	struct cam_sim *sim;
	struct cam_periph *periph;
	struct ctlfe_lun_softc *softc;

	ccb = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	sim = xpt_path_sim(ccb->ccb_h.path);

	mtx_lock(sim->mtx);

	periph = xpt_path_periph(ccb->ccb_h.path);

	softc = (struct ctlfe_lun_softc *)periph->softc;

	if (io->io_hdr.io_type == CTL_IO_TASK) {
		/*
		 * Task management commands don't require any further
		 * communication back to the adapter.  Requeue the CCB
		 * to the adapter, and free the CTL I/O.
		 */
		xpt_print(ccb->ccb_h.path, "%s: returning task I/O "
			  "tag %#x seq %#x\n", __func__,
			  ccb->cin1.tag_id, ccb->cin1.seq_id);
		/*
		 * Send the notify acknowledge down to the SIM, to let it
		 * know we processed the task management command.
		 */
		ccb->ccb_h.status = CAM_REQ_INPROG;
		ccb->ccb_h.func_code = XPT_NOTIFY_ACKNOWLEDGE;
		xpt_action(ccb);
		ctl_free_io(io);
	} else {
		if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE)
			io->io_hdr.flags |= CTL_FLAG_STATUS_QUEUED;
		else
			io->io_hdr.flags |= CTL_FLAG_DMA_QUEUED;

		TAILQ_INSERT_TAIL(&softc->work_queue, &ccb->ccb_h,
				  periph_links.tqe);

		/*
		 * Reset the timeout for our latest active DMA.
		 */
		callout_reset(&softc->dma_callout,
			      CTLFE_DMA_TIMEOUT * hz,
			      ctlfe_dma_timeout, softc);
		/*
		 * Ask for the CAM transport layer to send us a CCB to do
		 * the DMA or send status, unless ctlfe_dma_enabled is set
		 * to 0.
		 */
		if (ctlfe_dma_enabled != 0)
			xpt_schedule(periph, /*priority*/ 1);
	}

	mtx_unlock(sim->mtx);
}

static void
ctlfe_dump(void)
{
	struct ctlfe_softc *bus_softc;

	STAILQ_FOREACH(bus_softc, &ctlfe_softc_list, links) {
		struct ctlfe_lun_softc *lun_softc;

		ctlfe_dump_sim(bus_softc->sim);

		STAILQ_FOREACH(lun_softc, &bus_softc->lun_softc_list, links) {
			ctlfe_dump_queue(lun_softc);
		}
	}
}
