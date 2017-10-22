/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/ath/if_ath_rx.c 251655 2013-06-12 14:52:57Z adrian $");

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#include "opt_inet.h"
#include "opt_ath.h"
/*
 * This is needed for register operations which are performed
 * by the driver - eg, calls to ath_hal_gettsf32().
 *
 * It's also required for any AH_DEBUG checks in here, eg the
 * module dependencies.
 */
#include "opt_ah.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>
#include <sys/module.h>
#include <sys/ktr.h>
#include <sys/smp.h>	/* for mp_ncpus */

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#ifdef IEEE80211_SUPPORT_TDMA
#include <net80211/ieee80211_tdma.h>
#endif

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#include <dev/ath/if_ath_debug.h>
#include <dev/ath/if_ath_misc.h>
#include <dev/ath/if_ath_tsf.h>
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_sysctl.h>
#include <dev/ath/if_ath_led.h>
#include <dev/ath/if_ath_keycache.h>
#include <dev/ath/if_ath_rx.h>
#include <dev/ath/if_ath_beacon.h>
#include <dev/ath/if_athdfs.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#ifdef	ATH_DEBUG_ALQ
#include <dev/ath/if_ath_alq.h>
#endif

#include <dev/ath/if_ath_lna_div.h>

/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o accept PHY error frames when hardware doesn't have MIB support
 *   to count and we need them for ANI (sta mode only until recently)
 *   and we are not scanning (ANI is disabled)
 *   NB: older hal's add rx filter bits out of sight and we need to
 *	 blindly preserve them
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, mesh, or monitor modes
 * o enable promiscuous mode
 *   - when in monitor mode
 *   - if interface marked PROMISC (assumes bridge setting is filtered)
 * o accept beacons:
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when scanning
 *   - when doing s/w beacon miss (e.g. for ap+sta)
 *   - when operating in ap mode in 11g to detect overlapping bss that
 *     require protection
 *   - when operating in mesh mode to detect neighbors
 * o accept control frames:
 *   - when in monitor mode
 * XXX HT protection for 11n
 */
u_int32_t
ath_calcrxfilter(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	u_int32_t rfilt;

	rfilt = HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST;
	if (!sc->sc_needmib && !sc->sc_scanning)
		rfilt |= HAL_RX_FILTER_PHYERR;
	if (ic->ic_opmode != IEEE80211_M_STA)
		rfilt |= HAL_RX_FILTER_PROBEREQ;
	/* XXX ic->ic_monvaps != 0? */
	if (ic->ic_opmode == IEEE80211_M_MONITOR || (ifp->if_flags & IFF_PROMISC))
		rfilt |= HAL_RX_FILTER_PROM;
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_IBSS ||
	    sc->sc_swbmiss || sc->sc_scanning)
		rfilt |= HAL_RX_FILTER_BEACON;
	/*
	 * NB: We don't recalculate the rx filter when
	 * ic_protmode changes; otherwise we could do
	 * this only when ic_protmode != NONE.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    IEEE80211_IS_CHAN_ANYG(ic->ic_curchan))
		rfilt |= HAL_RX_FILTER_BEACON;

	/*
	 * Enable hardware PS-POLL RX only for hostap mode;
	 * STA mode sends PS-POLL frames but never
	 * receives them.
	 */
	if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_PSPOLL,
	    0, NULL) == HAL_OK &&
	    ic->ic_opmode == IEEE80211_M_HOSTAP)
		rfilt |= HAL_RX_FILTER_PSPOLL;

	if (sc->sc_nmeshvaps) {
		rfilt |= HAL_RX_FILTER_BEACON;
		if (sc->sc_hasbmatch)
			rfilt |= HAL_RX_FILTER_BSSID;
		else
			rfilt |= HAL_RX_FILTER_PROM;
	}
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		rfilt |= HAL_RX_FILTER_CONTROL;

	/*
	 * Enable RX of compressed BAR frames only when doing
	 * 802.11n. Required for A-MPDU.
	 */
	if (IEEE80211_IS_CHAN_HT(ic->ic_curchan))
		rfilt |= HAL_RX_FILTER_COMPBAR;

	/*
	 * Enable radar PHY errors if requested by the
	 * DFS module.
	 */
	if (sc->sc_dodfs)
		rfilt |= HAL_RX_FILTER_PHYRADAR;

	/*
	 * Enable spectral PHY errors if requested by the
	 * spectral module.
	 */
	if (sc->sc_dospectral)
		rfilt |= HAL_RX_FILTER_PHYRADAR;

	DPRINTF(sc, ATH_DEBUG_MODE, "%s: RX filter 0x%x, %s if_flags 0x%x\n",
	    __func__, rfilt, ieee80211_opmode_name[ic->ic_opmode], ifp->if_flags);
	return rfilt;
}

static int
ath_legacy_rxbuf_init(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	int error;
	struct mbuf *m;
	struct ath_desc *ds;

	m = bf->bf_m;
	if (m == NULL) {
		/*
		 * NB: by assigning a page to the rx dma buffer we
		 * implicitly satisfy the Atheros requirement that
		 * this buffer be cache-line-aligned and sized to be
		 * multiple of the cache line size.  Not doing this
		 * causes weird stuff to happen (for the 5210 at least).
		 */
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: no mbuf/cluster\n", __func__);
			sc->sc_stats.ast_rx_nombuf++;
			return ENOMEM;
		}
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

		error = bus_dmamap_load_mbuf_sg(sc->sc_dmat,
					     bf->bf_dmamap, m,
					     bf->bf_segs, &bf->bf_nseg,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_ANY,
			    "%s: bus_dmamap_load_mbuf_sg failed; error %d\n",
			    __func__, error);
			sc->sc_stats.ast_rx_busdma++;
			m_freem(m);
			return error;
		}
		KASSERT(bf->bf_nseg == 1,
			("multi-segment packet; nseg %u", bf->bf_nseg));
		bf->bf_m = m;
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREREAD);

	/*
	 * Setup descriptors.  For receive we always terminate
	 * the descriptor list with a self-linked entry so we'll
	 * not get overrun under high load (as can happen with a
	 * 5212 when ANI processing enables PHY error frames).
	 *
	 * To insure the last descriptor is self-linked we create
	 * each descriptor as self-linked and add it to the end.  As
	 * each additional descriptor is added the previous self-linked
	 * entry is ``fixed'' naturally.  This should be safe even
	 * if DMA is happening.  When processing RX interrupts we
	 * never remove/process the last, self-linked, entry on the
	 * descriptor list.  This insures the hardware always has
	 * someplace to write a new frame.
	 */
	/*
	 * 11N: we can no longer afford to self link the last descriptor.
	 * MAC acknowledges BA status as long as it copies frames to host
	 * buffer (or rx fifo). This can incorrectly acknowledge packets
	 * to a sender if last desc is self-linked.
	 */
	ds = bf->bf_desc;
	if (sc->sc_rxslink)
		ds->ds_link = bf->bf_daddr;	/* link to self */
	else
		ds->ds_link = 0;		/* terminate the list */
	ds->ds_data = bf->bf_segs[0].ds_addr;
	ath_hal_setuprxdesc(ah, ds
		, m->m_len		/* buffer size */
		, 0
	);

	if (sc->sc_rxlink != NULL)
		*sc->sc_rxlink = bf->bf_daddr;
	sc->sc_rxlink = &ds->ds_link;
	return 0;
}

/*
 * Intercept management frames to collect beacon rssi data
 * and to do ibss merges.
 */
void
ath_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m,
	int subtype, int rssi, int nf)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_softc *sc = vap->iv_ic->ic_ifp->if_softc;

	/*
	 * Call up first so subsequent work can use information
	 * potentially stored in the node (e.g. for ibss merge).
	 */
	ATH_VAP(vap)->av_recv_mgmt(ni, m, subtype, rssi, nf);
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_BEACON:
		/* update rssi statistics for use by the hal */
		/* XXX unlocked check against vap->iv_bss? */
		ATH_RSSI_LPF(sc->sc_halstats.ns_avgbrssi, rssi);
		if (sc->sc_syncbeacon &&
		    ni == vap->iv_bss && vap->iv_state == IEEE80211_S_RUN) {
			/*
			 * Resync beacon timers using the tsf of the beacon
			 * frame we just received.
			 */
			ath_beacon_config(sc, vap);
		}
		/* fall thru... */
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		if (vap->iv_opmode == IEEE80211_M_IBSS &&
		    vap->iv_state == IEEE80211_S_RUN) {
			uint32_t rstamp = sc->sc_lastrs->rs_tstamp;
			uint64_t tsf = ath_extend_tsf(sc, rstamp,
				ath_hal_gettsf64(sc->sc_ah));
			/*
			 * Handle ibss merge as needed; check the tsf on the
			 * frame before attempting the merge.  The 802.11 spec
			 * says the station should change it's bssid to match
			 * the oldest station with the same ssid, where oldest
			 * is determined by the tsf.  Note that hardware
			 * reconfiguration happens through callback to
			 * ath_newstate as the state machine will go from
			 * RUN -> RUN when this happens.
			 */
			if (le64toh(ni->ni_tstamp.tsf) >= tsf) {
				DPRINTF(sc, ATH_DEBUG_STATE,
				    "ibss merge, rstamp %u tsf %ju "
				    "tstamp %ju\n", rstamp, (uintmax_t)tsf,
				    (uintmax_t)ni->ni_tstamp.tsf);
				(void) ieee80211_ibss_merge(ni);
			}
		}
		break;
	}
}

#ifdef	ATH_ENABLE_RADIOTAP_VENDOR_EXT
static void
ath_rx_tap_vendor(struct ifnet *ifp, struct mbuf *m,
    const struct ath_rx_status *rs, u_int64_t tsf, int16_t nf)
{
	struct ath_softc *sc = ifp->if_softc;

	/* Fill in the extension bitmap */
	sc->sc_rx_th.wr_ext_bitmap = htole32(1 << ATH_RADIOTAP_VENDOR_HEADER);

	/* Fill in the vendor header */
	sc->sc_rx_th.wr_vh.vh_oui[0] = 0x7f;
	sc->sc_rx_th.wr_vh.vh_oui[1] = 0x03;
	sc->sc_rx_th.wr_vh.vh_oui[2] = 0x00;

	/* XXX what should this be? */
	sc->sc_rx_th.wr_vh.vh_sub_ns = 0;
	sc->sc_rx_th.wr_vh.vh_skip_len =
	    htole16(sizeof(struct ath_radiotap_vendor_hdr));

	/* General version info */
	sc->sc_rx_th.wr_v.vh_version = 1;

	sc->sc_rx_th.wr_v.vh_rx_chainmask = sc->sc_rxchainmask;

	/* rssi */
	sc->sc_rx_th.wr_v.rssi_ctl[0] = rs->rs_rssi_ctl[0];
	sc->sc_rx_th.wr_v.rssi_ctl[1] = rs->rs_rssi_ctl[1];
	sc->sc_rx_th.wr_v.rssi_ctl[2] = rs->rs_rssi_ctl[2];
	sc->sc_rx_th.wr_v.rssi_ext[0] = rs->rs_rssi_ext[0];
	sc->sc_rx_th.wr_v.rssi_ext[1] = rs->rs_rssi_ext[1];
	sc->sc_rx_th.wr_v.rssi_ext[2] = rs->rs_rssi_ext[2];

	/* evm */
	sc->sc_rx_th.wr_v.evm[0] = rs->rs_evm0;
	sc->sc_rx_th.wr_v.evm[1] = rs->rs_evm1;
	sc->sc_rx_th.wr_v.evm[2] = rs->rs_evm2;
	/* These are only populated from the AR9300 or later */
	sc->sc_rx_th.wr_v.evm[3] = rs->rs_evm3;
	sc->sc_rx_th.wr_v.evm[4] = rs->rs_evm4;

	/* direction */
	sc->sc_rx_th.wr_v.vh_flags = ATH_VENDOR_PKT_RX;

	/* RX rate */
	sc->sc_rx_th.wr_v.vh_rx_hwrate = rs->rs_rate;

	/* RX flags */
	sc->sc_rx_th.wr_v.vh_rs_flags = rs->rs_flags;

	if (rs->rs_isaggr)
		sc->sc_rx_th.wr_v.vh_flags |= ATH_VENDOR_PKT_ISAGGR;
	if (rs->rs_moreaggr)
		sc->sc_rx_th.wr_v.vh_flags |= ATH_VENDOR_PKT_MOREAGGR;

	/* phyerr info */
	if (rs->rs_status & HAL_RXERR_PHY) {
		sc->sc_rx_th.wr_v.vh_phyerr_code = rs->rs_phyerr;
		sc->sc_rx_th.wr_v.vh_flags |= ATH_VENDOR_PKT_RXPHYERR;
	} else {
		sc->sc_rx_th.wr_v.vh_phyerr_code = 0xff;
	}
	sc->sc_rx_th.wr_v.vh_rs_status = rs->rs_status;
	sc->sc_rx_th.wr_v.vh_rssi = rs->rs_rssi;
}
#endif	/* ATH_ENABLE_RADIOTAP_VENDOR_EXT */

static void
ath_rx_tap(struct ifnet *ifp, struct mbuf *m,
	const struct ath_rx_status *rs, u_int64_t tsf, int16_t nf)
{
#define	CHAN_HT20	htole32(IEEE80211_CHAN_HT20)
#define	CHAN_HT40U	htole32(IEEE80211_CHAN_HT40U)
#define	CHAN_HT40D	htole32(IEEE80211_CHAN_HT40D)
#define	CHAN_HT		(CHAN_HT20|CHAN_HT40U|CHAN_HT40D)
	struct ath_softc *sc = ifp->if_softc;
	const HAL_RATE_TABLE *rt;
	uint8_t rix;

	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	rix = rt->rateCodeToIndex[rs->rs_rate];
	sc->sc_rx_th.wr_rate = sc->sc_hwmap[rix].ieeerate;
	sc->sc_rx_th.wr_flags = sc->sc_hwmap[rix].rxflags;
#ifdef AH_SUPPORT_AR5416
	sc->sc_rx_th.wr_chan_flags &= ~CHAN_HT;
	if (rs->rs_status & HAL_RXERR_PHY) {
		/*
		 * PHY error - make sure the channel flags
		 * reflect the actual channel configuration,
		 * not the received frame.
		 */
		if (IEEE80211_IS_CHAN_HT40U(sc->sc_curchan))
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT40U;
		else if (IEEE80211_IS_CHAN_HT40D(sc->sc_curchan))
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT40D;
		else if (IEEE80211_IS_CHAN_HT20(sc->sc_curchan))
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT20;
	} else if (sc->sc_rx_th.wr_rate & IEEE80211_RATE_MCS) {	/* HT rate */
		struct ieee80211com *ic = ifp->if_l2com;

		if ((rs->rs_flags & HAL_RX_2040) == 0)
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT20;
		else if (IEEE80211_IS_CHAN_HT40U(ic->ic_curchan))
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT40U;
		else
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT40D;
		if ((rs->rs_flags & HAL_RX_GI) == 0)
			sc->sc_rx_th.wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;
	}

#endif
	sc->sc_rx_th.wr_tsf = htole64(ath_extend_tsf(sc, rs->rs_tstamp, tsf));
	if (rs->rs_status & HAL_RXERR_CRC)
		sc->sc_rx_th.wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
	/* XXX propagate other error flags from descriptor */
	sc->sc_rx_th.wr_antnoise = nf;
	sc->sc_rx_th.wr_antsignal = nf + rs->rs_rssi;
	sc->sc_rx_th.wr_antenna = rs->rs_antenna;
#undef CHAN_HT
#undef CHAN_HT20
#undef CHAN_HT40U
#undef CHAN_HT40D
}

static void
ath_handle_micerror(struct ieee80211com *ic,
	struct ieee80211_frame *wh, int keyix)
{
	struct ieee80211_node *ni;

	/* XXX recheck MIC to deal w/ chips that lie */
	/* XXX discard MIC errors on !data frames */
	ni = ieee80211_find_rxnode(ic, (const struct ieee80211_frame_min *) wh);
	if (ni != NULL) {
		ieee80211_notify_michael_failure(ni->ni_vap, wh, keyix);
		ieee80211_free_node(ni);
	}
}

/*
 * Process a single packet.
 *
 * The mbuf must already be synced, unmapped and removed from bf->bf_m
 * by this stage.
 *
 * The mbuf must be consumed by this routine - either passed up the
 * net80211 stack, put on the holding queue, or freed.
 */
int
ath_rx_pkt(struct ath_softc *sc, struct ath_rx_status *rs, HAL_STATUS status,
    uint64_t tsf, int nf, HAL_RX_QUEUE qtype, struct ath_buf *bf,
    struct mbuf *m)
{
	uint64_t rstamp;
	int len, type;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_node *ni;
	int is_good = 0;
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];

	/*
	 * Calculate the correct 64 bit TSF given
	 * the TSF64 register value and rs_tstamp.
	 */
	rstamp = ath_extend_tsf(sc, rs->rs_tstamp, tsf);

	/* These aren't specifically errors */
#ifdef	AH_SUPPORT_AR5416
	if (rs->rs_flags & HAL_RX_GI)
		sc->sc_stats.ast_rx_halfgi++;
	if (rs->rs_flags & HAL_RX_2040)
		sc->sc_stats.ast_rx_2040++;
	if (rs->rs_flags & HAL_RX_DELIM_CRC_PRE)
		sc->sc_stats.ast_rx_pre_crc_err++;
	if (rs->rs_flags & HAL_RX_DELIM_CRC_POST)
		sc->sc_stats.ast_rx_post_crc_err++;
	if (rs->rs_flags & HAL_RX_DECRYPT_BUSY)
		sc->sc_stats.ast_rx_decrypt_busy_err++;
	if (rs->rs_flags & HAL_RX_HI_RX_CHAIN)
		sc->sc_stats.ast_rx_hi_rx_chain++;
	if (rs->rs_flags & HAL_RX_STBC)
		sc->sc_stats.ast_rx_stbc++;
#endif /* AH_SUPPORT_AR5416 */

	if (rs->rs_status != 0) {
		if (rs->rs_status & HAL_RXERR_CRC)
			sc->sc_stats.ast_rx_crcerr++;
		if (rs->rs_status & HAL_RXERR_FIFO)
			sc->sc_stats.ast_rx_fifoerr++;
		if (rs->rs_status & HAL_RXERR_PHY) {
			sc->sc_stats.ast_rx_phyerr++;
			/* Process DFS radar events */
			if ((rs->rs_phyerr == HAL_PHYERR_RADAR) ||
			    (rs->rs_phyerr == HAL_PHYERR_FALSE_RADAR_EXT)) {
				/* Now pass it to the radar processing code */
				ath_dfs_process_phy_err(sc, m, rstamp, rs);
			}

			/* Be suitably paranoid about receiving phy errors out of the stats array bounds */
			if (rs->rs_phyerr < 64)
				sc->sc_stats.ast_rx_phy[rs->rs_phyerr]++;
			goto rx_error;	/* NB: don't count in ierrors */
		}
		if (rs->rs_status & HAL_RXERR_DECRYPT) {
			/*
			 * Decrypt error.  If the error occurred
			 * because there was no hardware key, then
			 * let the frame through so the upper layers
			 * can process it.  This is necessary for 5210
			 * parts which have no way to setup a ``clear''
			 * key cache entry.
			 *
			 * XXX do key cache faulting
			 */
			if (rs->rs_keyix == HAL_RXKEYIX_INVALID)
				goto rx_accept;
			sc->sc_stats.ast_rx_badcrypt++;
		}
		/*
		 * Similar as above - if the failure was a keymiss
		 * just punt it up to the upper layers for now.
		 */
		if (rs->rs_status & HAL_RXERR_KEYMISS) {
			sc->sc_stats.ast_rx_keymiss++;
			goto rx_accept;
		}
		if (rs->rs_status & HAL_RXERR_MIC) {
			sc->sc_stats.ast_rx_badmic++;
			/*
			 * Do minimal work required to hand off
			 * the 802.11 header for notification.
			 */
			/* XXX frag's and qos frames */
			len = rs->rs_datalen;
			if (len >= sizeof (struct ieee80211_frame)) {
				ath_handle_micerror(ic,
				    mtod(m, struct ieee80211_frame *),
				    sc->sc_splitmic ?
					rs->rs_keyix-32 : rs->rs_keyix);
			}
		}
		ifp->if_ierrors++;
rx_error:
		/*
		 * Cleanup any pending partial frame.
		 */
		if (re->m_rxpending != NULL) {
			m_freem(re->m_rxpending);
			re->m_rxpending = NULL;
		}
		/*
		 * When a tap is present pass error frames
		 * that have been requested.  By default we
		 * pass decrypt+mic errors but others may be
		 * interesting (e.g. crc).
		 */
		if (ieee80211_radiotap_active(ic) &&
		    (rs->rs_status & sc->sc_monpass)) {
			/* NB: bpf needs the mbuf length setup */
			len = rs->rs_datalen;
			m->m_pkthdr.len = m->m_len = len;
			ath_rx_tap(ifp, m, rs, rstamp, nf);
#ifdef	ATH_ENABLE_RADIOTAP_VENDOR_EXT
			ath_rx_tap_vendor(ifp, m, rs, rstamp, nf);
#endif	/* ATH_ENABLE_RADIOTAP_VENDOR_EXT */
			ieee80211_radiotap_rx_all(ic, m);
		}
		/* XXX pass MIC errors up for s/w reclaculation */
		m_freem(m); m = NULL;
		goto rx_next;
	}
rx_accept:
	len = rs->rs_datalen;
	m->m_len = len;

	if (rs->rs_more) {
		/*
		 * Frame spans multiple descriptors; save
		 * it for the next completed descriptor, it
		 * will be used to construct a jumbogram.
		 */
		if (re->m_rxpending != NULL) {
			/* NB: max frame size is currently 2 clusters */
			sc->sc_stats.ast_rx_toobig++;
			m_freem(re->m_rxpending);
		}
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;
		re->m_rxpending = m;
		m = NULL;
		goto rx_next;
	} else if (re->m_rxpending != NULL) {
		/*
		 * This is the second part of a jumbogram,
		 * chain it to the first mbuf, adjust the
		 * frame length, and clear the rxpending state.
		 */
		re->m_rxpending->m_next = m;
		re->m_rxpending->m_pkthdr.len += len;
		m = re->m_rxpending;
		re->m_rxpending = NULL;
	} else {
		/*
		 * Normal single-descriptor receive; setup
		 * the rcvif and packet length.
		 */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;
	}

	/*
	 * Validate rs->rs_antenna.
	 *
	 * Some users w/ AR9285 NICs have reported crashes
	 * here because rs_antenna field is bogusly large.
	 * Let's enforce the maximum antenna limit of 8
	 * (and it shouldn't be hard coded, but that's a
	 * separate problem) and if there's an issue, print
	 * out an error and adjust rs_antenna to something
	 * sensible.
	 *
	 * This code should be removed once the actual
	 * root cause of the issue has been identified.
	 * For example, it may be that the rs_antenna
	 * field is only valid for the lsat frame of
	 * an aggregate and it just happens that it is
	 * "mostly" right. (This is a general statement -
	 * the majority of the statistics are only valid
	 * for the last frame in an aggregate.
	 */
	if (rs->rs_antenna > 7) {
		device_printf(sc->sc_dev, "%s: rs_antenna > 7 (%d)\n",
		    __func__, rs->rs_antenna);
#ifdef	ATH_DEBUG
		ath_printrxbuf(sc, bf, 0, status == HAL_OK);
#endif /* ATH_DEBUG */
		rs->rs_antenna = 0;	/* XXX better than nothing */
	}

	/*
	 * If this is an AR9285/AR9485, then the receive and LNA
	 * configuration is stored in RSSI[2] / EXTRSSI[2].
	 * We can extract this out to build a much better
	 * receive antenna profile.
	 *
	 * Yes, this just blurts over the above RX antenna field
	 * for now.  It's fine, the AR9285 doesn't really use
	 * that.
	 *
	 * Later on we should store away the fine grained LNA
	 * information and keep separate counters just for
	 * that.  It'll help when debugging the AR9285/AR9485
	 * combined diversity code.
	 */
	if (sc->sc_rx_lnamixer) {
		rs->rs_antenna = 0;

		/* Bits 0:1 - the LNA configuration used */
		rs->rs_antenna |=
		    ((rs->rs_rssi_ctl[2] & HAL_RX_LNA_CFG_USED)
		      >> HAL_RX_LNA_CFG_USED_S);

		/* Bit 2 - the external RX antenna switch */
		if (rs->rs_rssi_ctl[2] & HAL_RX_LNA_EXTCFG)
			rs->rs_antenna |= 0x4;
	}

	ifp->if_ipackets++;
	sc->sc_stats.ast_ant_rx[rs->rs_antenna]++;

	/*
	 * Populate the rx status block.  When there are bpf
	 * listeners we do the additional work to provide
	 * complete status.  Otherwise we fill in only the
	 * material required by ieee80211_input.  Note that
	 * noise setting is filled in above.
	 */
	if (ieee80211_radiotap_active(ic)) {
		ath_rx_tap(ifp, m, rs, rstamp, nf);
#ifdef	ATH_ENABLE_RADIOTAP_VENDOR_EXT
		ath_rx_tap_vendor(ifp, m, rs, rstamp, nf);
#endif	/* ATH_ENABLE_RADIOTAP_VENDOR_EXT */
	}

	/*
	 * From this point on we assume the frame is at least
	 * as large as ieee80211_frame_min; verify that.
	 */
	if (len < IEEE80211_MIN_LEN) {
		if (!ieee80211_radiotap_active(ic)) {
			DPRINTF(sc, ATH_DEBUG_RECV,
			    "%s: short packet %d\n", __func__, len);
			sc->sc_stats.ast_rx_tooshort++;
		} else {
			/* NB: in particular this captures ack's */
			ieee80211_radiotap_rx_all(ic, m);
		}
		m_freem(m); m = NULL;
		goto rx_next;
	}

	if (IFF_DUMPPKTS(sc, ATH_DEBUG_RECV)) {
		const HAL_RATE_TABLE *rt = sc->sc_currates;
		uint8_t rix = rt->rateCodeToIndex[rs->rs_rate];

		ieee80211_dump_pkt(ic, mtod(m, caddr_t), len,
		    sc->sc_hwmap[rix].ieeerate, rs->rs_rssi);
	}

	m_adj(m, -IEEE80211_CRC_LEN);

	/*
	 * Locate the node for sender, track state, and then
	 * pass the (referenced) node up to the 802.11 layer
	 * for its use.
	 */
	ni = ieee80211_find_rxnode_withkey(ic,
		mtod(m, const struct ieee80211_frame_min *),
		rs->rs_keyix == HAL_RXKEYIX_INVALID ?
			IEEE80211_KEYIX_NONE : rs->rs_keyix);
	sc->sc_lastrs = rs;

#ifdef	AH_SUPPORT_AR5416
	if (rs->rs_isaggr)
		sc->sc_stats.ast_rx_agg++;
#endif /* AH_SUPPORT_AR5416 */

	if (ni != NULL) {
		/*
		 * Only punt packets for ampdu reorder processing for
		 * 11n nodes; net80211 enforces that M_AMPDU is only
		 * set for 11n nodes.
		 */
		if (ni->ni_flags & IEEE80211_NODE_HT)
			m->m_flags |= M_AMPDU;

		/*
		 * Sending station is known, dispatch directly.
		 */
		type = ieee80211_input(ni, m, rs->rs_rssi, nf);
		ieee80211_free_node(ni);
		m = NULL;
		/*
		 * Arrange to update the last rx timestamp only for
		 * frames from our ap when operating in station mode.
		 * This assumes the rx key is always setup when
		 * associated.
		 */
		if (ic->ic_opmode == IEEE80211_M_STA &&
		    rs->rs_keyix != HAL_RXKEYIX_INVALID)
			is_good = 1;
	} else {
		type = ieee80211_input_all(ic, m, rs->rs_rssi, nf);
		m = NULL;
	}

	/*
	 * At this point we have passed the frame up the stack; thus
	 * the mbuf is no longer ours.
	 */

	/*
	 * Track rx rssi and do any rx antenna management.
	 */
	ATH_RSSI_LPF(sc->sc_halstats.ns_avgrssi, rs->rs_rssi);
	if (sc->sc_diversity) {
		/*
		 * When using fast diversity, change the default rx
		 * antenna if diversity chooses the other antenna 3
		 * times in a row.
		 */
		if (sc->sc_defant != rs->rs_antenna) {
			if (++sc->sc_rxotherant >= 3)
				ath_setdefantenna(sc, rs->rs_antenna);
		} else
			sc->sc_rxotherant = 0;
	}

	/* Handle slow diversity if enabled */
	if (sc->sc_dolnadiv) {
		ath_lna_rx_comb_scan(sc, rs, ticks, hz);
	}

	if (sc->sc_softled) {
		/*
		 * Blink for any data frame.  Otherwise do a
		 * heartbeat-style blink when idle.  The latter
		 * is mainly for station mode where we depend on
		 * periodic beacon frames to trigger the poll event.
		 */
		if (type == IEEE80211_FC0_TYPE_DATA) {
			const HAL_RATE_TABLE *rt = sc->sc_currates;
			ath_led_event(sc,
			    rt->rateCodeToIndex[rs->rs_rate]);
		} else if (ticks - sc->sc_ledevent >= sc->sc_ledidle)
			ath_led_event(sc, 0);
		}
rx_next:
	/*
	 * Debugging - complain if we didn't NULL the mbuf pointer
	 * here.
	 */
	if (m != NULL) {
		device_printf(sc->sc_dev,
		    "%s: mbuf %p should've been freed!\n",
		    __func__,
		    m);
	}
	return (is_good);
}

#define	ATH_RX_MAX		128

static void
ath_rx_proc(struct ath_softc *sc, int resched)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc + \
		((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
	struct ath_buf *bf;
	struct ifnet *ifp = sc->sc_ifp;
	struct ath_hal *ah = sc->sc_ah;
#ifdef IEEE80211_SUPPORT_SUPERG
	struct ieee80211com *ic = ifp->if_l2com;
#endif
	struct ath_desc *ds;
	struct ath_rx_status *rs;
	struct mbuf *m;
	int ngood;
	HAL_STATUS status;
	int16_t nf;
	u_int64_t tsf;
	int npkts = 0;
	int kickpcu = 0;

	/* XXX we must not hold the ATH_LOCK here */
	ATH_UNLOCK_ASSERT(sc);
	ATH_PCU_UNLOCK_ASSERT(sc);

	ATH_PCU_LOCK(sc);
	sc->sc_rxproc_cnt++;
	kickpcu = sc->sc_kickpcu;
	ATH_PCU_UNLOCK(sc);

	DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: called\n", __func__);
	ngood = 0;
	nf = ath_hal_getchannoise(ah, sc->sc_curchan);
	sc->sc_stats.ast_rx_noise = nf;
	tsf = ath_hal_gettsf64(ah);
	do {
		/*
		 * Don't process too many packets at a time; give the
		 * TX thread time to also run - otherwise the TX
		 * latency can jump by quite a bit, causing throughput
		 * degredation.
		 */
		if (!kickpcu && npkts >= ATH_RX_MAX)
			break;

		bf = TAILQ_FIRST(&sc->sc_rxbuf);
		if (sc->sc_rxslink && bf == NULL) {	/* NB: shouldn't happen */
			if_printf(ifp, "%s: no buffer!\n", __func__);
			break;
		} else if (bf == NULL) {
			/*
			 * End of List:
			 * this can happen for non-self-linked RX chains
			 */
			sc->sc_stats.ast_rx_hitqueueend++;
			break;
		}
		m = bf->bf_m;
		if (m == NULL) {		/* NB: shouldn't happen */
			/*
			 * If mbuf allocation failed previously there
			 * will be no mbuf; try again to re-populate it.
			 */
			/* XXX make debug msg */
			if_printf(ifp, "%s: no mbuf!\n", __func__);
			TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);
			goto rx_proc_next;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr) {
			/* NB: never process the self-linked entry at the end */
			sc->sc_stats.ast_rx_hitqueueend++;
			break;
		}
		/* XXX sync descriptor memory */
		/*
		 * Must provide the virtual address of the current
		 * descriptor, the physical address, and the virtual
		 * address of the next descriptor in the h/w chain.
		 * This allows the HAL to look ahead to see if the
		 * hardware is done with a descriptor by checking the
		 * done bit in the following descriptor and the address
		 * of the current descriptor the DMA engine is working
		 * on.  All this is necessary because of our use of
		 * a self-linked list to avoid rx overruns.
		 */
		rs = &bf->bf_status.ds_rxstat;
		status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link), rs);
#ifdef ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(sc, bf, 0, status == HAL_OK);
#endif

#ifdef	ATH_DEBUG_ALQ
		if (if_ath_alq_checkdebug(&sc->sc_alq, ATH_ALQ_EDMA_RXSTATUS))
		    if_ath_alq_post(&sc->sc_alq, ATH_ALQ_EDMA_RXSTATUS,
		    sc->sc_rx_statuslen, (char *) ds);
#endif	/* ATH_DEBUG_ALQ */

		if (status == HAL_EINPROGRESS)
			break;

		TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);
		npkts++;

		/*
		 * Process a single frame.
		 */
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bf->bf_m = NULL;
		if (ath_rx_pkt(sc, rs, status, tsf, nf, HAL_RX_QUEUE_HP, bf, m))
			ngood++;
rx_proc_next:
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	} while (ath_rxbuf_init(sc, bf) == 0);

	/* rx signal state monitoring */
	ath_hal_rxmonitor(ah, &sc->sc_halstats, sc->sc_curchan);
	if (ngood)
		sc->sc_lastrx = tsf;

	ATH_KTR(sc, ATH_KTR_RXPROC, 2, "ath_rx_proc: npkts=%d, ngood=%d", npkts, ngood);
	/* Queue DFS tasklet if needed */
	if (resched && ath_dfs_tasklet_needed(sc, sc->sc_curchan))
		taskqueue_enqueue(sc->sc_tq, &sc->sc_dfstask);

	/*
	 * Now that all the RX frames were handled that
	 * need to be handled, kick the PCU if there's
	 * been an RXEOL condition.
	 */
	if (resched && kickpcu) {
		ATH_PCU_LOCK(sc);
		ATH_KTR(sc, ATH_KTR_ERROR, 0, "ath_rx_proc: kickpcu");
		device_printf(sc->sc_dev, "%s: kickpcu; handled %d packets\n",
		    __func__, npkts);

		/*
		 * Go through the process of fully tearing down
		 * the RX buffers and reinitialising them.
		 *
		 * There's a hardware bug that causes the RX FIFO
		 * to get confused under certain conditions and
		 * constantly write over the same frame, leading
		 * the RX driver code here to get heavily confused.
		 */
#if 1
		ath_startrecv(sc);
#else
		/*
		 * Disabled for now - it'd be nice to be able to do
		 * this in order to limit the amount of CPU time spent
		 * reinitialising the RX side (and thus minimise RX
		 * drops) however there's a hardware issue that
		 * causes things to get too far out of whack.
		 */
		/*
		 * XXX can we hold the PCU lock here?
		 * Are there any net80211 buffer calls involved?
		 */
		bf = TAILQ_FIRST(&sc->sc_rxbuf);
		ath_hal_putrxbuf(ah, bf->bf_daddr, HAL_RX_QUEUE_HP);
		ath_hal_rxena(ah);		/* enable recv descriptors */
		ath_mode_init(sc);		/* set filters, etc. */
		ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
#endif

		ath_hal_intrset(ah, sc->sc_imask);
		sc->sc_kickpcu = 0;
		ATH_PCU_UNLOCK(sc);
	}

	/* XXX check this inside of IF_LOCK? */
	if (resched && (ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
#ifdef IEEE80211_SUPPORT_SUPERG
		ieee80211_ff_age_all(ic, 100);
#endif
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			ath_tx_kick(sc);
	}
#undef PA2DESC

	/*
	 * If we hit the maximum number of frames in this round,
	 * reschedule for another immediate pass.  This gives
	 * the TX and TX completion routines time to run, which
	 * will reduce latency.
	 */
	if (npkts >= ATH_RX_MAX)
		sc->sc_rx.recv_sched(sc, resched);

	ATH_PCU_LOCK(sc);
	sc->sc_rxproc_cnt--;
	ATH_PCU_UNLOCK(sc);
}

#undef	ATH_RX_MAX

/*
 * Only run the RX proc if it's not already running.
 * Since this may get run as part of the reset/flush path,
 * the task can't clash with an existing, running tasklet.
 */
static void
ath_legacy_rx_tasklet(void *arg, int npending)
{
	struct ath_softc *sc = arg;

	ATH_KTR(sc, ATH_KTR_RXPROC, 1, "ath_rx_proc: pending=%d", npending);
	DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: pending %u\n", __func__, npending);
	ATH_PCU_LOCK(sc);
	if (sc->sc_inreset_cnt > 0) {
		device_printf(sc->sc_dev,
		    "%s: sc_inreset_cnt > 0; skipping\n", __func__);
		ATH_PCU_UNLOCK(sc);
		return;
	}
	ATH_PCU_UNLOCK(sc);

	ath_rx_proc(sc, 1);
}

static void
ath_legacy_flushrecv(struct ath_softc *sc)
{

	ath_rx_proc(sc, 0);
}

/*
 * Disable the receive h/w in preparation for a reset.
 */
static void
ath_legacy_stoprecv(struct ath_softc *sc, int dodelay)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc + \
		((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_stoppcurecv(ah);	/* disable PCU */
	ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
	ath_hal_stopdmarecv(ah);	/* disable DMA engine */
	/*
	 * TODO: see if this particular DELAY() is required; it may be
	 * masking some missing FIFO flush or DMA sync.
	 */
#if 0
	if (dodelay)
#endif
		DELAY(3000);		/* 3ms is long enough for 1 frame */
#ifdef ATH_DEBUG
	if (sc->sc_debug & (ATH_DEBUG_RESET | ATH_DEBUG_FATAL)) {
		struct ath_buf *bf;
		u_int ix;

		device_printf(sc->sc_dev,
		    "%s: rx queue %p, link %p\n",
		    __func__,
		    (caddr_t)(uintptr_t) ath_hal_getrxbuf(ah, HAL_RX_QUEUE_HP),
		    sc->sc_rxlink);
		ix = 0;
		TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
			struct ath_desc *ds = bf->bf_desc;
			struct ath_rx_status *rs = &bf->bf_status.ds_rxstat;
			HAL_STATUS status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link), rs);
			if (status == HAL_OK || (sc->sc_debug & ATH_DEBUG_FATAL))
				ath_printrxbuf(sc, bf, ix, status == HAL_OK);
			ix++;
		}
	}
#endif
	/*
	 * Free both high/low RX pending, just in case.
	 */
	if (sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending != NULL) {
		m_freem(sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending);
		sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending = NULL;
	}
	if (sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending != NULL) {
		m_freem(sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending);
		sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending = NULL;
	}
	sc->sc_rxlink = NULL;		/* just in case */
#undef PA2DESC
}

/*
 * Enable the receive h/w following a reset.
 */
static int
ath_legacy_startrecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;

	sc->sc_rxlink = NULL;
	sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending = NULL;
	sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending = NULL;
	TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		int error = ath_rxbuf_init(sc, bf);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_RECV,
				"%s: ath_rxbuf_init failed %d\n",
				__func__, error);
			return error;
		}
	}

	bf = TAILQ_FIRST(&sc->sc_rxbuf);
	ath_hal_putrxbuf(ah, bf->bf_daddr, HAL_RX_QUEUE_HP);
	ath_hal_rxena(ah);		/* enable recv descriptors */
	ath_mode_init(sc);		/* set filters, etc. */
	ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
	return 0;
}

static int
ath_legacy_dma_rxsetup(struct ath_softc *sc)
{
	int error;

	error = ath_descdma_setup(sc, &sc->sc_rxdma, &sc->sc_rxbuf,
	    "rx", sizeof(struct ath_desc), ath_rxbuf, 1);
	if (error != 0)
		return (error);

	return (0);
}

static int
ath_legacy_dma_rxteardown(struct ath_softc *sc)
{

	if (sc->sc_rxdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
	return (0);
}

static void
ath_legacy_recv_sched(struct ath_softc *sc, int dosched)
{

	taskqueue_enqueue(sc->sc_tq, &sc->sc_rxtask);
}

static void
ath_legacy_recv_sched_queue(struct ath_softc *sc, HAL_RX_QUEUE q,
    int dosched)
{

	taskqueue_enqueue(sc->sc_tq, &sc->sc_rxtask);
}

void
ath_recv_setup_legacy(struct ath_softc *sc)
{

	/* Sensible legacy defaults */
	/*
	 * XXX this should be changed to properly support the
	 * exact RX descriptor size for each HAL.
	 */
	sc->sc_rx_statuslen = sizeof(struct ath_desc);

	sc->sc_rx.recv_start = ath_legacy_startrecv;
	sc->sc_rx.recv_stop = ath_legacy_stoprecv;
	sc->sc_rx.recv_flush = ath_legacy_flushrecv;
	sc->sc_rx.recv_tasklet = ath_legacy_rx_tasklet;
	sc->sc_rx.recv_rxbuf_init = ath_legacy_rxbuf_init;

	sc->sc_rx.recv_setup = ath_legacy_dma_rxsetup;
	sc->sc_rx.recv_teardown = ath_legacy_dma_rxteardown;
	sc->sc_rx.recv_sched = ath_legacy_recv_sched;
	sc->sc_rx.recv_sched_queue = ath_legacy_recv_sched_queue;
}
