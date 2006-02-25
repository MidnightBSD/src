/*-
 * FreeBSD OSM Library for the aic7xxx aic79xx based Adaptec SCSI controllers
 *
 * Copyright (c) 1994-2002 Justin T. Gibbs.
 * Copyright (c) 2001-2003 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
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
 *
 * $Id: aic_osm_lib.c,v 1.1.1.2 2006-02-25 02:36:18 laffer1 Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/aic7xxx/aic_osm_lib.c,v 1.4 2005/01/06 01:42:26 imp Exp $");

static void	aic_recovery_thread(void *arg);

void
aic_set_recoveryscb(struct aic_softc *aic, struct scb *scb)
{

	if ((scb->flags & SCB_RECOVERY_SCB) == 0) {
		struct scb *list_scb;

		scb->flags |= SCB_RECOVERY_SCB;

		AIC_SCB_DATA(aic)->recovery_scbs++;

		/*
		 * Go through all of our pending SCBs and remove
		 * any scheduled timeouts for them.  We will reschedule
		 * them after we've successfully fixed this problem.
		 */
		LIST_FOREACH(list_scb, &aic->pending_scbs, pending_links) {
			union ccb *ccb;

			ccb = list_scb->io_ctx;
			untimeout(aic_platform_timeout, list_scb,
				  ccb->ccb_h.timeout_ch);
		}
	}
}

void
aic_platform_timeout(void *arg)
{
	struct	scb *scb;
	u_long	s;
	
	scb = (struct scb *)arg; 
	aic_lock(scb->aic_softc, &s);
	aic_timeout(scb);
	aic_unlock(scb->aic_softc, &s);
}

int
aic_spawn_recovery_thread(struct aic_softc *aic)
{
	int error;

	error = aic_kthread_create(aic_recovery_thread, aic,
			       &aic->platform_data->recovery_thread,
			       /*flags*/0, /*altstack*/0, "aic_recovery%d",
			       aic->unit);
	return (error);
}

/*
 * Lock is not held on entry.
 */
void
aic_terminate_recovery_thread(struct aic_softc *aic)
{
	u_long s;

	aic_lock(aic, &s);
	if (aic->platform_data->recovery_thread == NULL) {
		aic_unlock(aic, &s);
		return;
	}
	aic->flags |= AIC_SHUTDOWN_RECOVERY;
	wakeup(aic);
	/*
	 * Sleep on a slightly different location 
	 * for this interlock just for added safety.
	 */
	tsleep(aic->platform_data, PUSER, "thtrm", 0);
	aic_unlock(aic, &s);
}

static void
aic_recovery_thread(void *arg)
{
	struct aic_softc *aic;
	u_long s;

#if __FreeBSD_version >= 500000
	mtx_lock(&Giant);
#endif
	aic = (struct aic_softc *)arg;
	aic_lock(aic, &s);
	for (;;) {
		
		if (LIST_EMPTY(&aic->timedout_scbs) != 0
		 && (aic->flags & AIC_SHUTDOWN_RECOVERY) == 0)
			tsleep(aic, PUSER, "idle", 0);

		if ((aic->flags & AIC_SHUTDOWN_RECOVERY) != 0)
			break;

		aic_unlock(aic, &s);
		aic_recover_commands(aic);
		aic_lock(aic, &s);
	}
	aic->platform_data->recovery_thread = NULL;
	wakeup(aic->platform_data);
	aic_unlock(aic, &s);
#if __FreeBSD_version >= 500000
	mtx_unlock(&Giant);
#endif
	kthread_exit(0);
}

void
aic_calc_geometry(struct ccb_calc_geometry *ccg, int extended)
{
#if __FreeBSD_version >= 500000
	cam_calc_geometry(ccg, extended);
#else
	uint32_t size_mb;
	uint32_t secs_per_cylinder;

	size_mb = ccg->volume_size / ((1024L * 1024L) / ccg->block_size);
	if (size_mb > 1024 && extended) {
		ccg->heads = 255;
		ccg->secs_per_track = 63;
	} else {
		ccg->heads = 64;
		ccg->secs_per_track = 32;
	}
	secs_per_cylinder = ccg->heads * ccg->secs_per_track;
	ccg->cylinders = ccg->volume_size / secs_per_cylinder;
	ccg->ccb_h.status = CAM_REQ_CMP;
#endif
}

