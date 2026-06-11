/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bitstring.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/iflib.h>

#include "aq_common.h"
#include "aq_dbg.h"
#include "aq_device.h"
#include "aq_fw.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_ring.h"

int
aq_update_hw_stats(aq_dev_t *aq_dev)
{
	struct aq_hw *hw = &aq_dev->hw;
	struct aq_stats_s stats;
	bool aq2_b0 = AQ_HW_IS_AQ2(hw) &&
	    hw->aq2_iface_ver == AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_B0;
	int err;

	err = aq_hw_mpi_read_stats(hw, &stats);
	if (err != 0)
		return (err);

	if (!aq_dev->last_stats_valid) {
		memcpy(&aq_dev->last_stats, &stats, sizeof(aq_dev->last_stats));
		aq_dev->last_stats_valid = true;
		return (0);
	}

#define AQ_SDELTA_32(_N_)                                 \
	(aq_dev->accum_stats._N_ += (uint32_t)stats._N_ - \
		(uint32_t)aq_dev->last_stats._N_)
#define AQ_SDELTA_64(_N_) \
	(aq_dev->accum_stats._N_ += stats._N_ - aq_dev->last_stats._N_)

	if (aq_dev->linkup) {
		if (aq2_b0) {
			AQ_SDELTA_64(ucast_pkts_rcvd);
			AQ_SDELTA_64(mcast_pkts_rcvd);
			AQ_SDELTA_64(bcast_pkts_rcvd);
			AQ_SDELTA_64(err_pkts_rcvd);

			AQ_SDELTA_64(ucast_pkts_txd);
			AQ_SDELTA_64(mcast_pkts_txd);
			AQ_SDELTA_64(bcast_pkts_txd);
			AQ_SDELTA_64(err_pkts_txd);

			AQ_SDELTA_64(good_octets_rcvd);
			AQ_SDELTA_64(good_octets_txd);
			AQ_SDELTA_64(pause_frames_rcvd);
			AQ_SDELTA_64(pause_frames_txd);

			AQ_SDELTA_32(rsc_pkts_rcvd);
			AQ_SDELTA_32(drop_pkts_dma);

			aq_dev->accum_stats.good_pkts_rcvd =
			    aq_dev->accum_stats.ucast_pkts_rcvd +
			    aq_dev->accum_stats.mcast_pkts_rcvd +
			    aq_dev->accum_stats.bcast_pkts_rcvd;
			aq_dev->accum_stats.good_pkts_txd =
			    aq_dev->accum_stats.ucast_pkts_txd +
			    aq_dev->accum_stats.mcast_pkts_txd +
			    aq_dev->accum_stats.bcast_pkts_txd;
		} else {
			AQ_SDELTA_32(ucast_pkts_rcvd);
			AQ_SDELTA_32(mcast_pkts_rcvd);
			AQ_SDELTA_32(bcast_pkts_rcvd);
			AQ_SDELTA_32(rsc_pkts_rcvd);
			AQ_SDELTA_32(err_pkts_rcvd);

			AQ_SDELTA_32(ucast_pkts_txd);
			AQ_SDELTA_32(mcast_pkts_txd);
			AQ_SDELTA_32(bcast_pkts_txd);
			AQ_SDELTA_32(err_pkts_txd);

			AQ_SDELTA_32(ucast_octets_rcvd);
			AQ_SDELTA_32(mcast_octets_rcvd);
			AQ_SDELTA_32(bcast_octets_rcvd);

			AQ_SDELTA_32(ucast_octets_txd);
			AQ_SDELTA_32(mcast_octets_txd);
			AQ_SDELTA_32(bcast_octets_txd);

			aq_dev->accum_stats.good_octets_rcvd =
			    aq_dev->accum_stats.ucast_octets_rcvd +
			    aq_dev->accum_stats.mcast_octets_rcvd +
			    aq_dev->accum_stats.bcast_octets_rcvd;
			aq_dev->accum_stats.good_octets_txd =
			    aq_dev->accum_stats.ucast_octets_txd +
			    aq_dev->accum_stats.mcast_octets_txd +
			    aq_dev->accum_stats.bcast_octets_txd;
			AQ_SDELTA_32(good_pkts_rcvd);
			AQ_SDELTA_32(good_pkts_txd);

			AQ_SDELTA_32(drop_pkts_dma);
			AQ_SDELTA_32(pause_frames_rcvd);
			AQ_SDELTA_32(pause_frames_txd);
		}
	}
#undef AQ_SDELTA_64
#undef AQ_SDELTA_32

	memcpy(&aq_dev->last_stats, &stats, sizeof(aq_dev->last_stats));

	return (0);
}

void
aq_if_update_admin_status(if_ctx_t ctx)
{
	aq_dev_t *aq_dev = iflib_get_softc(ctx);
	struct aq_hw *hw = &aq_dev->hw;
	if_t ifp = iflib_get_ifp(ctx);
	bool link_changed;
	uint32_t old_link_speed;
	uint32_t link_speed;

	//	AQ_DBG_ENTER();

	struct aq_hw_fc_info fc_neg;
	aq_hw_get_link_state(hw, &link_speed, &fc_neg);
	old_link_speed = (uint32_t)(if_getbaudrate(ifp) / IF_Mbps(1));
	link_changed = (link_speed != old_link_speed);

	if (link_speed && (!aq_dev->linkup || link_changed)) {
		if (!aq_dev->linkup) {
			device_printf(aq_dev->dev,
			    "atlantic: link UP: speed=%d\n", link_speed);
		} else {
			device_printf(aq_dev->dev,
			    "atlantic: link speed change: %u -> %u\n",
			    old_link_speed, link_speed);
		}

		aq_dev->linkup = true;

#if __FreeBSD__ >= 12
		/* Disable TSO if link speed < 1G */
		if (link_speed < 1000 &&
		    (iflib_get_softc_ctx(ctx)->isc_capabilities &
			(IFCAP_TSO4 | IFCAP_TSO6))) {
			iflib_get_softc_ctx(ctx)->isc_capabilities &= ~(
			    IFCAP_TSO4 | IFCAP_TSO6);
			device_printf(aq_dev->dev,
			    "atlantic: TSO disabled for link speed < 1G");
		} else {
			iflib_get_softc_ctx(ctx)->isc_capabilities |=
			    (IFCAP_TSO4 | IFCAP_TSO6);
		}
#endif

		/* turn on/off RX Pause in RPB */
		rpb_rx_xoff_en_per_tc_set(hw, fc_neg.fc_rx, 0);

		iflib_link_state_change(ctx, LINK_STATE_UP,
		    IF_Mbps(link_speed));
		aq_mediastatus_update(aq_dev, link_speed, &fc_neg);

		/* update ITR settings according new link speed */
		aq_hw_interrupt_moderation_set(hw);
	} else if (link_speed == 0U && aq_dev->linkup) { /* link was UP */
		device_printf(aq_dev->dev, "atlantic: link DOWN\n");

		aq_dev->linkup = false;

		/* turn off RX Pause in RPB */
		rpb_rx_xoff_en_per_tc_set(hw, 0, 0);

		iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
		aq_mediastatus_update(aq_dev, link_speed, &fc_neg);
	}

	aq_update_hw_stats(aq_dev);
	//	AQ_DBG_EXIT(0);
}

/**************************************************************************/
/* interrupt service routine  (Top half)                                  */
/**************************************************************************/
int
aq_isr_rx(void *arg)
{
	struct aq_ring *ring = arg;
	struct aq_dev *aq_dev = ring->dev;
	struct aq_hw *hw = &aq_dev->hw;

	/* clear interrupt status */
	itr_irq_status_clearlsw_set(hw, BIT(ring->msix));
	ring->stats.irq++;
	return (FILTER_SCHEDULE_THREAD);
}

/**************************************************************************/
/* interrupt service routine  (Top half)                                  */
/**************************************************************************/
int
aq_linkstat_isr(void *arg)
{
	aq_dev_t *aq_dev = arg;
	struct aq_hw *hw = &aq_dev->hw;

	/* clear interrupt status */
	itr_irq_status_clearlsw_set(hw, BIT(aq_dev->msix));

	iflib_admin_intr_deferred(aq_dev->ctx);

	return (FILTER_HANDLED);
}
