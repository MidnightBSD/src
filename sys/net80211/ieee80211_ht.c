/*-
 * Copyright (c) 2007 Sam Leffler, Errno Consulting
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
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: release/7.0.0/sys/net80211/ieee80211_ht.c 175998 2008-02-05 18:19:12Z sam $");
#endif

/*
 * IEEE 802.11n protocol support.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h> 
#include <sys/endian.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

/* define here, used throughout file */
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)

/* XXX need max array size */
/* NB: these are for HT20 w/ long GI */
const int ieee80211_htrates[16] = {
	13,		/* IFM_IEEE80211_MCS0 */
	26,		/* IFM_IEEE80211_MCS1 */
	39,		/* IFM_IEEE80211_MCS2 */
	52,		/* IFM_IEEE80211_MCS3 */
	78,		/* IFM_IEEE80211_MCS4 */
	104,		/* IFM_IEEE80211_MCS5 */
	117,		/* IFM_IEEE80211_MCS6 */
	130,		/* IFM_IEEE80211_MCS7 */
	26,		/* IFM_IEEE80211_MCS8 */
	52,		/* IFM_IEEE80211_MCS9 */
	78,		/* IFM_IEEE80211_MCS10 */
	104,		/* IFM_IEEE80211_MCS11 */
	156,		/* IFM_IEEE80211_MCS12 */
	208,		/* IFM_IEEE80211_MCS13 */
	234,		/* IFM_IEEE80211_MCS14 */
	260,		/* IFM_IEEE80211_MCS15 */
};

static const struct ieee80211_htrateset ieee80211_rateset_11n =
	{ 16, {
	/* MCS: 6.5   13 19.5   26   39  52 58.5  65  13  26 */
	          0,   1,   2,   3,   4,  5,   6,  7,  8,  9,
	/*       39   52   78  104  117, 130 */
		 10,  11,  12,  13,  14,  15 }
	};

#ifdef IEEE80211_AMPDU_AGE
/* XXX public for sysctl hookup */
int	ieee80211_ampdu_age = -1;	/* threshold for ampdu reorder q (ms) */
#endif
int	ieee80211_recv_bar_ena = 1;

#define	IEEE80211_AGGR_TIMEOUT	msecs_to_ticks(250)
#define	IEEE80211_AGGR_MINRETRY	msecs_to_ticks(10*1000)
#define	IEEE80211_AGGR_MAXTRIES	3

static int ieee80211_addba_request(struct ieee80211_node *ni,
	struct ieee80211_tx_ampdu *tap,
	int dialogtoken, int baparamset, int batimeout);
static int ieee80211_addba_response(struct ieee80211_node *ni,
	struct ieee80211_tx_ampdu *tap,
	int code, int baparamset, int batimeout);
static void ieee80211_addba_stop(struct ieee80211_node *ni,
	struct ieee80211_tx_ampdu *tap);
static void ieee80211_aggr_recv_action(struct ieee80211_node *ni,
	const uint8_t *frm, const uint8_t *efrm);

void
ieee80211_ht_attach(struct ieee80211com *ic)
{
#ifdef IEEE80211_AMPDU_AGE
	if (ieee80211_ampdu_age == -1)
		ieee80211_ampdu_age = msecs_to_ticks(500);
#endif

	/* setup default aggregation policy */
	ic->ic_recv_action = ieee80211_aggr_recv_action;
	ic->ic_send_action = ieee80211_send_action;
	ic->ic_addba_request = ieee80211_addba_request;
	ic->ic_addba_response = ieee80211_addba_response;
	ic->ic_addba_stop = ieee80211_addba_stop;

	ic->ic_htprotmode = IEEE80211_PROT_RTSCTS;
	ic->ic_curhtprotmode = IEEE80211_HTINFO_OPMODE_PURE;

	/* XXX get from driver */
	ic->ic_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_8K;
	ic->ic_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_NA;
	ic->ic_ampdu_limit = ic->ic_ampdu_rxmax;
	ic->ic_amsdu_limit = IEEE80211_HTCAP_MAXAMSDU_3839;

	if (ic->ic_htcaps & IEEE80211_HTC_HT) {
		/*
		 * Device is HT capable; enable all HT-related
		 * facilities by default.
		 * XXX these choices may be too aggressive.
		 */
		ic->ic_flags_ext |= IEEE80211_FEXT_HT
				 |  IEEE80211_FEXT_HTCOMPAT
				 ;
		if (ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI20)
			ic->ic_flags_ext |= IEEE80211_FEXT_SHORTGI20;
		/* XXX infer from channel list? */
		if (ic->ic_htcaps & IEEE80211_HTCAP_CHWIDTH40) {
			ic->ic_flags_ext |= IEEE80211_FEXT_USEHT40;
			if (ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI40)
				ic->ic_flags_ext |= IEEE80211_FEXT_SHORTGI40;
		}
		/* NB: A-MPDU and A-MSDU rx are mandated, these are tx only */
		ic->ic_flags_ext |= IEEE80211_FEXT_AMPDU_RX;
		if (ic->ic_htcaps & IEEE80211_HTC_AMPDU)
			ic->ic_flags_ext |= IEEE80211_FEXT_AMPDU_TX;
		ic->ic_flags_ext |= IEEE80211_FEXT_AMSDU_RX;
		if (ic->ic_htcaps & IEEE80211_HTC_AMSDU)
			ic->ic_flags_ext |= IEEE80211_FEXT_AMSDU_TX;
	}
}

void
ieee80211_ht_detach(struct ieee80211com *ic)
{
}

static void
ht_announce(struct ieee80211com *ic, int mode,
	const struct ieee80211_htrateset *rs)
{
	struct ifnet *ifp = ic->ic_ifp;
	int i, rate, mword;

	if_printf(ifp, "%s MCS: ", ieee80211_phymode_name[mode]);
	for (i = 0; i < rs->rs_nrates; i++) {
		mword = ieee80211_rate2media(ic,
		    rs->rs_rates[i] | IEEE80211_RATE_MCS, mode);
		if (IFM_SUBTYPE(mword) != IFM_IEEE80211_MCS)
			continue;
		rate = ieee80211_htrates[rs->rs_rates[i]];
		printf("%s%d%sMbps", (i != 0 ? " " : ""),
		    rate / 2, ((rate & 0x1) != 0 ? ".5" : ""));
	}
	printf("\n");
}

void
ieee80211_ht_announce(struct ieee80211com *ic)
{
	if (isset(ic->ic_modecaps, IEEE80211_MODE_11NA))
		ht_announce(ic, IEEE80211_MODE_11NA, &ieee80211_rateset_11n);
	if (isset(ic->ic_modecaps, IEEE80211_MODE_11NG))
		ht_announce(ic, IEEE80211_MODE_11NG, &ieee80211_rateset_11n);
}

const struct ieee80211_htrateset *
ieee80211_get_suphtrates(struct ieee80211com *ic,
	const struct ieee80211_channel *c)
{
	return &ieee80211_rateset_11n;
}

/*
 * Receive processing.
 */

/*
 * Decap the encapsulated A-MSDU frames and dispatch all but
 * the last for delivery.  The last frame is returned for 
 * delivery via the normal path.
 */
struct mbuf *
ieee80211_decap_amsdu(struct ieee80211_node *ni, struct mbuf *m)
{
	struct ieee80211com *ic = ni->ni_ic;
	int framelen;
	struct mbuf *n;

	/* discard 802.3 header inserted by ieee80211_decap */
	m_adj(m, sizeof(struct ether_header));

	ic->ic_stats.is_amsdu_decap++;

	for (;;) {
		/*
		 * Decap the first frame, bust it apart from the
		 * remainder and deliver.  We leave the last frame
		 * delivery to the caller (for consistency with other
		 * code paths, could also do it here).
		 */
		m = ieee80211_decap1(m, &framelen);
		if (m == NULL) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, "a-msdu", "%s", "decap failed");
			ic->ic_stats.is_amsdu_tooshort++;
			return NULL;
		}
		if (m->m_pkthdr.len == framelen)
			break;
		n = m_split(m, framelen, M_NOWAIT);
		if (n == NULL) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, "a-msdu",
			    "%s", "unable to split encapsulated frames");
			ic->ic_stats.is_amsdu_split++;
			m_freem(m);			/* NB: must reclaim */
			return NULL;
		}
		ieee80211_deliver_data(ic, ni, m);

		/*
		 * Remove frame contents; each intermediate frame
		 * is required to be aligned to a 4-byte boundary.
		 */
		m = n;
		m_adj(m, roundup2(framelen, 4) - framelen);	/* padding */
	}
	return m;				/* last delivered by caller */
}

/*
 * Start A-MPDU rx/re-order processing for the specified TID.
 */
static void
ampdu_rx_start(struct ieee80211_rx_ampdu *rap, int bufsiz, int start)
{
	memset(rap, 0, sizeof(*rap));
	rap->rxa_wnd = (bufsiz == 0) ?
	    IEEE80211_AGGR_BAWMAX : min(bufsiz, IEEE80211_AGGR_BAWMAX);
	rap->rxa_start = start;
	rap->rxa_flags |= IEEE80211_AGGR_XCHGPEND;
}

/*
 * Purge all frames in the A-MPDU re-order queue.
 */
static void
ampdu_rx_purge(struct ieee80211_rx_ampdu *rap)
{
	struct mbuf *m;
	int i;

	for (i = 0; i < rap->rxa_wnd; i++) {
		m = rap->rxa_m[i];
		if (m != NULL) {
			rap->rxa_m[i] = NULL;
			rap->rxa_qbytes -= m->m_pkthdr.len;
			m_freem(m);
			if (--rap->rxa_qframes == 0)
				break;
		}
	}
	KASSERT(rap->rxa_qbytes == 0 && rap->rxa_qframes == 0,
	    ("lost %u data, %u frames on ampdu rx q",
	    rap->rxa_qbytes, rap->rxa_qframes));
}

/*
 * Stop A-MPDU rx processing for the specified TID.
 */
static void
ampdu_rx_stop(struct ieee80211_rx_ampdu *rap)
{
	rap->rxa_flags &= ~IEEE80211_AGGR_XCHGPEND;
	ampdu_rx_purge(rap);
}

/*
 * Dispatch a frame from the A-MPDU reorder queue.  The
 * frame is fed back into ieee80211_input marked with an
 * M_AMPDU flag so it doesn't come back to us (it also
 * permits ieee80211_input to optimize re-processing).
 */
static __inline void
ampdu_dispatch(struct ieee80211_node *ni, struct mbuf *m)
{
	m->m_flags |= M_AMPDU;	/* bypass normal processing */
	/* NB: rssi, noise, and rstamp are ignored w/ M_AMPDU set */
	(void) ieee80211_input(ni->ni_ic, m, ni, 0, 0, 0);
}

/*
 * Dispatch as many frames as possible from the re-order queue.
 * Frames will always be "at the front"; we process all frames
 * up to the first empty slot in the window.  On completion we
 * cleanup state if there are still pending frames in the current
 * BA window.  We assume the frame at slot 0 is already handled
 * by the caller; we always start at slot 1.
 */
static void
ampdu_rx_dispatch(struct ieee80211_rx_ampdu *rap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m;
	int i;

	/* flush run of frames */
	for (i = 1; i < rap->rxa_wnd; i++) {
		m = rap->rxa_m[i];
		if (m == NULL)
			break;
		rap->rxa_m[i] = NULL;
		rap->rxa_qbytes -= m->m_pkthdr.len;
		rap->rxa_qframes--;

		ampdu_dispatch(ni, m);
	}
	/*
	 * If frames remain, copy the mbuf pointers down so
	 * they correspond to the offsets in the new window.
	 */
	if (rap->rxa_qframes != 0) {
		int n = rap->rxa_qframes, j;
		for (j = i+1; j < rap->rxa_wnd; j++) {
			if (rap->rxa_m[j] != NULL) {
				rap->rxa_m[j-i] = rap->rxa_m[j];
				rap->rxa_m[j] = NULL;
				if (--n == 0)
					break;
			}
		}
		KASSERT(n == 0, ("lost %d frames", n));
		ic->ic_stats.is_ampdu_rx_copy += rap->rxa_qframes;
	}
	/*
	 * Adjust the start of the BA window to
	 * reflect the frames just dispatched.
	 */
	rap->rxa_start = IEEE80211_SEQ_ADD(rap->rxa_start, i);
	ic->ic_stats.is_ampdu_rx_oor += i;
}

#ifdef IEEE80211_AMPDU_AGE
/*
 * Dispatch all frames in the A-MPDU re-order queue.
 */
static void
ampdu_rx_flush(struct ieee80211_node *ni, struct ieee80211_rx_ampdu *rap)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m;
	int i;

	for (i = 0; i < rap->rxa_wnd; i++) {
		m = rap->rxa_m[i];
		if (m == NULL)
			continue;
		rap->rxa_m[i] = NULL;
		rap->rxa_qbytes -= m->m_pkthdr.len;
		rap->rxa_qframes--;
		ic->ic_stats.is_ampdu_rx_oor++;

		ampdu_dispatch(ni, m);
		if (rap->rxa_qframes == 0)
			break;
	}
}
#endif /* IEEE80211_AMPDU_AGE */

/*
 * Dispatch all frames in the A-MPDU re-order queue
 * preceding the specified sequence number.  This logic
 * handles window moves due to a received MSDU or BAR.
 */
static void
ampdu_rx_flush_upto(struct ieee80211_node *ni,
	struct ieee80211_rx_ampdu *rap, ieee80211_seq winstart)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m;
	ieee80211_seq seqno;
	int i;

	/*
	 * Flush any complete MSDU's with a sequence number lower
	 * than winstart.  Gaps may exist.  Note that we may actually
	 * dispatch frames past winstart if a run continues; this is
	 * an optimization that avoids having to do a separate pass
	 * to dispatch frames after moving the BA window start.
	 */
	seqno = rap->rxa_start;
	for (i = 0; i < rap->rxa_wnd; i++) {
		m = rap->rxa_m[i];
		if (m != NULL) {
			rap->rxa_m[i] = NULL;
			rap->rxa_qbytes -= m->m_pkthdr.len;
			rap->rxa_qframes--;
			ic->ic_stats.is_ampdu_rx_oor++;

			ampdu_dispatch(ni, m);
		} else {
			if (!IEEE80211_SEQ_BA_BEFORE(seqno, winstart))
				break;
		}
		seqno = IEEE80211_SEQ_INC(seqno);
	}
	/*
	 * If frames remain, copy the mbuf pointers down so
	 * they correspond to the offsets in the new window.
	 */
	if (rap->rxa_qframes != 0) {
		int n = rap->rxa_qframes, j;
		for (j = i+1; j < rap->rxa_wnd; j++) {
			if (rap->rxa_m[j] != NULL) {
				rap->rxa_m[j-i] = rap->rxa_m[j];
				rap->rxa_m[j] = NULL;
				if (--n == 0)
					break;
			}
		}
		KASSERT(n == 0, ("%s: lost %d frames, qframes %d off %d "
		    "BA win <%d:%d> winstart %d",
		    __func__, n, rap->rxa_qframes, i, rap->rxa_start,
		    IEEE80211_SEQ_ADD(rap->rxa_start, rap->rxa_wnd-1),
		    winstart));
		ic->ic_stats.is_ampdu_rx_copy += rap->rxa_qframes;
	}
	/*
	 * Move the start of the BA window; we use the
	 * sequence number of the last MSDU that was
	 * passed up the stack+1 or winstart if stopped on
	 * a gap in the reorder buffer.
	 */
	rap->rxa_start = seqno;
}

/*
 * Process a received QoS data frame for an HT station.  Handle
 * A-MPDU reordering: if this frame is received out of order
 * and falls within the BA window hold onto it.  Otherwise if
 * this frame completes a run, flush any pending frames.  We
 * return 1 if the frame is consumed.  A 0 is returned if
 * the frame should be processed normally by the caller.
 */
int
ieee80211_ampdu_reorder(struct ieee80211_node *ni, struct mbuf *m)
{
#define	IEEE80211_FC0_QOSDATA \
	(IEEE80211_FC0_TYPE_DATA|IEEE80211_FC0_SUBTYPE_QOS|IEEE80211_FC0_VERSION_0)
#define	PROCESS		0	/* caller should process frame */
#define	CONSUMED	1	/* frame consumed, caller does nothing */
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_qosframe *wh;
	struct ieee80211_rx_ampdu *rap;
	ieee80211_seq rxseq;
	uint8_t tid;
	int off;

	KASSERT(ni->ni_flags & IEEE80211_NODE_HT, ("not an HT sta"));

	/* NB: m_len known to be sufficient */
	wh = mtod(m, struct ieee80211_qosframe *);
	KASSERT(wh->i_fc[0] == IEEE80211_FC0_QOSDATA, ("not QoS data"));

	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
		tid = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0];
	else
		tid = wh->i_qos[0];
	tid &= IEEE80211_QOS_TID;
	rap = &ni->ni_rx_ampdu[tid];
	if ((rap->rxa_flags & IEEE80211_AGGR_XCHGPEND) == 0) {
		/*
		 * No ADDBA request yet, don't touch.
		 */
		return PROCESS;
	}
	rxseq = le16toh(*(uint16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
	rap->rxa_nframes++;
again:
	if (rxseq == rap->rxa_start) {
		/*
		 * First frame in window.
		 */
		if (rap->rxa_qframes != 0) {
			/*
			 * Dispatch as many packets as we can.
			 */
			KASSERT(rap->rxa_m[0] == NULL, ("unexpected dup"));
			ampdu_dispatch(ni, m);
			ampdu_rx_dispatch(rap, ni);
			return CONSUMED;
		} else {
			/*
			 * In order; advance window and notify
			 * caller to dispatch directly.
			 */
			rap->rxa_start = IEEE80211_SEQ_INC(rxseq);
			return PROCESS;
		}
	}
	/*
	 * Frame is out of order; store if in the BA window.
	 */
	/* calculate offset in BA window */
	off = IEEE80211_SEQ_SUB(rxseq, rap->rxa_start);
	if (off < rap->rxa_wnd) {
		/*
		 * Common case (hopefully): in the BA window.
		 * Sec 9.10.7.6 a) (D2.04 p.118 line 47)
		 */
#ifdef IEEE80211_AMPDU_AGE
		/* 
		 * Check for frames sitting too long in the reorder queue.
		 * This should only ever happen if frames are not delivered
		 * without the sender otherwise notifying us (e.g. with a
		 * BAR to move the window).  Typically this happens because
		 * of vendor bugs that cause the sequence number to jump.
		 * When this happens we get a gap in the reorder queue that
		 * leaves frame sitting on the queue until they get pushed
		 * out due to window moves.  When the vendor does not send
		 * BAR this move only happens due to explicit packet sends
		 *
		 * NB: we only track the time of the oldest frame in the
		 * reorder q; this means that if we flush we might push
		 * frames that still "new"; if this happens then subsequent
		 * frames will result in BA window moves which cost something
		 * but is still better than a big throughput dip.
		 */
		if (rap->rxa_qframes != 0) {
			/* XXX honor batimeout? */
			if (ticks - rap->rxa_age > ieee80211_ampdu_age) {
				/*
				 * Too long since we received the first
				 * frame; flush the reorder buffer.
				 */
				if (rap->rxa_qframes != 0) {
					ic->ic_stats.is_ampdu_rx_age +=
					    rap->rxa_qframes;
					ampdu_rx_flush(ni, rap);
				}
				rap->rxa_start = IEEE80211_SEQ_INC(rxseq);
				return PROCESS;
			}
		} else {
			/*
			 * First frame, start aging timer.
			 */
			rap->rxa_age = ticks;
		}
#endif /* IEEE80211_AMPDU_AGE */
		/* save packet */
		if (rap->rxa_m[off] == NULL) {
			rap->rxa_m[off] = m;
			rap->rxa_qframes++;
			rap->rxa_qbytes += m->m_pkthdr.len;
			ic->ic_stats.is_ampdu_rx_reorder++;
		} else {
			IEEE80211_DISCARD_MAC(ic,
			    IEEE80211_MSG_INPUT | IEEE80211_MSG_11N,
			    ni->ni_macaddr, "a-mpdu duplicate",
			    "seqno %u tid %u BA win <%u:%u>",
			    rxseq, tid, rap->rxa_start,
			    IEEE80211_SEQ_ADD(rap->rxa_start, rap->rxa_wnd-1));
			ic->ic_stats.is_rx_dup++;
			IEEE80211_NODE_STAT(ni, rx_dup);
			m_freem(m);
		}
		return CONSUMED;
	}
	if (off < IEEE80211_SEQ_BA_RANGE) {
		/*
		 * Outside the BA window, but within range;
		 * flush the reorder q and move the window.
		 * Sec 9.10.7.6 b) (D2.04 p.118 line 60)
		 */
		IEEE80211_NOTE(ic, IEEE80211_MSG_11N, ni,
		    "move BA win <%u:%u> (%u frames) rxseq %u tid %u",
		    rap->rxa_start,
		    IEEE80211_SEQ_ADD(rap->rxa_start, rap->rxa_wnd-1),
		    rap->rxa_qframes, rxseq, tid);
		ic->ic_stats.is_ampdu_rx_move++;

		/*
		 * The spec says to flush frames up to but not including:
		 * 	WinStart_B = rxseq - rap->rxa_wnd + 1
		 * Then insert the frame or notify the caller to process
		 * it immediately.  We can safely do this by just starting
		 * over again because we know the frame will now be within
		 * the BA window.
		 */
		/* NB: rxa_wnd known to be >0 */
		ampdu_rx_flush_upto(ni, rap,
		    IEEE80211_SEQ_SUB(rxseq, rap->rxa_wnd-1));
		goto again;
	} else {
		/*
		 * Outside the BA window and out of range; toss.
		 * Sec 9.10.7.6 c) (D2.04 p.119 line 16)
		 */
		IEEE80211_DISCARD_MAC(ic,
		    IEEE80211_MSG_INPUT | IEEE80211_MSG_11N, ni->ni_macaddr,
		    "MSDU", "BA win <%u:%u> (%u frames) rxseq %u tid %u%s",
		    rap->rxa_start,
		    IEEE80211_SEQ_ADD(rap->rxa_start, rap->rxa_wnd-1),
		    rap->rxa_qframes, rxseq, tid,
		    wh->i_fc[1] & IEEE80211_FC1_RETRY ? " (retransmit)" : "");
		ic->ic_stats.is_ampdu_rx_drop++;
		IEEE80211_NODE_STAT(ni, rx_drop);
		m_freem(m);
		return CONSUMED;
	}
#undef CONSUMED
#undef PROCESS
#undef IEEE80211_FC0_QOSDATA
}

/*
 * Process a BAR ctl frame.  Dispatch all frames up to
 * the sequence number of the frame.  If this frame is
 * out of range it's discarded.
 */
void
ieee80211_recv_bar(struct ieee80211_node *ni, struct mbuf *m0)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame_bar *wh;
	struct ieee80211_rx_ampdu *rap;
	ieee80211_seq rxseq;
	int tid, off;

	if (!ieee80211_recv_bar_ena) {
#if 0
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_11N,
		    ni->ni_macaddr, "BAR", "%s", "processing disabled");
#endif
		ic->ic_stats.is_ampdu_bar_bad++;
		return;
	}
	wh = mtod(m0, struct ieee80211_frame_bar *);
	/* XXX check basic BAR */
	tid = MS(le16toh(wh->i_ctl), IEEE80211_BAR_TID);
	rap = &ni->ni_rx_ampdu[tid];
	if ((rap->rxa_flags & IEEE80211_AGGR_XCHGPEND) == 0) {
		/*
		 * No ADDBA request yet, don't touch.
		 */
		IEEE80211_DISCARD_MAC(ic,
		    IEEE80211_MSG_INPUT | IEEE80211_MSG_11N,
		    ni->ni_macaddr, "BAR", "no BA stream, tid %u", tid);
		ic->ic_stats.is_ampdu_bar_bad++;
		return;
	}
	ic->ic_stats.is_ampdu_bar_rx++;
	rxseq = le16toh(wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
	if (rxseq == rap->rxa_start)
		return;
	/* calculate offset in BA window */
	off = IEEE80211_SEQ_SUB(rxseq, rap->rxa_start);
	if (off < IEEE80211_SEQ_BA_RANGE) {
		/*
		 * Flush the reorder q up to rxseq and move the window.
		 * Sec 9.10.7.6 a) (D2.04 p.119 line 22)
		 */
		IEEE80211_NOTE(ic, IEEE80211_MSG_11N, ni,
		    "BAR moves BA win <%u:%u> (%u frames) rxseq %u tid %u",
		    rap->rxa_start,
		    IEEE80211_SEQ_ADD(rap->rxa_start, rap->rxa_wnd-1),
		    rap->rxa_qframes, rxseq, tid);
		ic->ic_stats.is_ampdu_bar_move++;

		ampdu_rx_flush_upto(ni, rap, rxseq);
		if (off >= rap->rxa_wnd) {
			/*
			 * BAR specifies a window start to the right of BA
			 * window; we must move it explicitly since
			 * ampdu_rx_flush_upto will not.
			 */
			rap->rxa_start = rxseq;
		}
	} else {
		/*
		 * Out of range; toss.
		 * Sec 9.10.7.6 b) (D2.04 p.119 line 41)
		 */
		IEEE80211_DISCARD_MAC(ic,
		    IEEE80211_MSG_INPUT | IEEE80211_MSG_11N, ni->ni_macaddr,
		    "BAR", "BA win <%u:%u> (%u frames) rxseq %u tid %u%s",
		    rap->rxa_start,
		    IEEE80211_SEQ_ADD(rap->rxa_start, rap->rxa_wnd-1),
		    rap->rxa_qframes, rxseq, tid,
		    wh->i_fc[1] & IEEE80211_FC1_RETRY ? " (retransmit)" : "");
		ic->ic_stats.is_ampdu_bar_oow++;
		IEEE80211_NODE_STAT(ni, rx_drop);
	}
}

/*
 * Setup HT-specific state in a node.  Called only
 * when HT use is negotiated so we don't do extra
 * work for temporary and/or legacy sta's.
 */
void
ieee80211_ht_node_init(struct ieee80211_node *ni, const uint8_t *htcap)
{
	struct ieee80211_tx_ampdu *tap;
	int ac;

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		/*
		 * Clean AMPDU state on re-associate.  This handles the case
		 * where a station leaves w/o notifying us and then returns
		 * before node is reaped for inactivity.
		 */
		ieee80211_ht_node_cleanup(ni);
	}
	ieee80211_parse_htcap(ni, htcap);
	for (ac = 0; ac < WME_NUM_AC; ac++) {
		tap = &ni->ni_tx_ampdu[ac];
		tap->txa_ac = ac;
		/* NB: further initialization deferred */
	}
	ni->ni_flags |= IEEE80211_NODE_HT | IEEE80211_NODE_AMPDU;
}

/*
 * Cleanup HT-specific state in a node.  Called only
 * when HT use has been marked.
 */
void
ieee80211_ht_node_cleanup(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	int i;

	KASSERT(ni->ni_flags & IEEE80211_NODE_HT, ("not an HT node"));

	/* XXX optimize this */
	for (i = 0; i < WME_NUM_AC; i++) {
		struct ieee80211_tx_ampdu *tap = &ni->ni_tx_ampdu[i];
		if (tap->txa_flags & IEEE80211_AGGR_SETUP) {
			/*
			 * Stop BA stream if setup so driver has a chance
			 * to reclaim any resources it might have allocated.
			 */
			ic->ic_addba_stop(ni, &ni->ni_tx_ampdu[i]);
			IEEE80211_TAPQ_DESTROY(tap);
			/* NB: clearing NAK means we may re-send ADDBA */ 
			tap->txa_flags &=
			    ~(IEEE80211_AGGR_SETUP | IEEE80211_AGGR_NAK);
		}
	}
	for (i = 0; i < WME_NUM_TID; i++)
		ampdu_rx_stop(&ni->ni_rx_ampdu[i]);

	ni->ni_htcap = 0;
	ni->ni_flags &= ~(IEEE80211_NODE_HT | IEEE80211_NODE_HTCOMPAT |
		IEEE80211_NODE_AMPDU);
}

static struct ieee80211_channel *
findhtchan(struct ieee80211com *ic, struct ieee80211_channel *c, int htflags)
{
	return ieee80211_find_channel(ic, c->ic_freq,
	    (c->ic_flags &~ IEEE80211_CHAN_HT) | htflags);
}

/*
 * Adjust a channel to be HT/non-HT according to the vap's configuration.
 */
struct ieee80211_channel *
ieee80211_ht_adjust_channel(struct ieee80211com *ic,
	struct ieee80211_channel *chan, int flags)
{
	struct ieee80211_channel *c;

	if (flags & IEEE80211_FEXT_HT) {
		/* promote to HT if possible */
		if (flags & IEEE80211_FEXT_USEHT40) {
			if (!IEEE80211_IS_CHAN_HT40(chan)) {
				/* NB: arbitrarily pick ht40+ over ht40- */
				c = findhtchan(ic, chan, IEEE80211_CHAN_HT40U);
				if (c == NULL)
					c = findhtchan(ic, chan,
						IEEE80211_CHAN_HT40D);
				if (c == NULL)
					c = findhtchan(ic, chan,
						IEEE80211_CHAN_HT20);
				if (c != NULL)
					chan = c;
			}
		} else if (!IEEE80211_IS_CHAN_HT20(chan)) {
			c = findhtchan(ic, chan, IEEE80211_CHAN_HT20);
			if (c != NULL)
				chan = c;
		}
	} else if (IEEE80211_IS_CHAN_HT(chan)) {
		/* demote to legacy, HT use is disabled */
		c = ieee80211_find_channel(ic, chan->ic_freq,
		    chan->ic_flags &~ IEEE80211_CHAN_HT);
		if (c != NULL)
			chan = c;
	}
	return chan;
}

/*
 * Setup HT-specific state for a legacy WDS peer.
 */
void
ieee80211_ht_wds_init(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_tx_ampdu *tap;
	int ac;

	KASSERT(ic->ic_flags_ext & IEEE80211_FEXT_HT, ("no HT requested"));

	/* XXX check scan cache in case peer has an ap and we have info */
	/*
	 * If setup with a legacy channel; locate an HT channel.
	 * Otherwise if the inherited channel (from a companion
	 * AP) is suitable use it so we use the same location
	 * for the extension channel).
	 */
	ni->ni_chan = ieee80211_ht_adjust_channel(ic, ni->ni_chan,
	    ic->ic_flags_ext);

	ni->ni_htcap = 0;
	if (ic->ic_flags_ext & IEEE80211_FEXT_SHORTGI20)
		ni->ni_htcap |= IEEE80211_HTCAP_SHORTGI20;
	if (IEEE80211_IS_CHAN_HT40(ni->ni_chan)) {
		ni->ni_htcap |= IEEE80211_HTCAP_CHWIDTH40;
		ni->ni_chw = 40;
		if (IEEE80211_IS_CHAN_HT40U(ni->ni_chan))
			ni->ni_ht2ndchan = IEEE80211_HTINFO_2NDCHAN_ABOVE;
		else if (IEEE80211_IS_CHAN_HT40D(ni->ni_chan))
			ni->ni_ht2ndchan = IEEE80211_HTINFO_2NDCHAN_BELOW;
		if (ic->ic_flags_ext & IEEE80211_FEXT_SHORTGI40)
			ni->ni_htcap |= IEEE80211_HTCAP_SHORTGI40;
	} else {
		ni->ni_chw = 20;
		ni->ni_ht2ndchan = IEEE80211_HTINFO_2NDCHAN_NONE;
	}
	ni->ni_htctlchan = ni->ni_chan->ic_ieee;

	ni->ni_htopmode = 0;		/* XXX need protection state */
	ni->ni_htstbc = 0;		/* XXX need info */

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		tap = &ni->ni_tx_ampdu[ac];
		tap->txa_ac = ac;
	}
	/* NB: AMPDU tx/rx governed by IEEE80211_FEXT_AMPDU_{TX,RX} */
	ni->ni_flags |= IEEE80211_NODE_HT | IEEE80211_NODE_AMPDU;
}

/*
 * Notify hostap vaps of a change in the HTINFO ie.
 */
static void
htinfo_notify(struct ieee80211com *ic)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		return;
	IEEE80211_NOTE(ic,
	    IEEE80211_MSG_ASSOC | IEEE80211_MSG_11N,
	    ic->ic_bss,
	    "HT bss occupancy change: %d sta, %d ht, "
	    "%d ht40%s, HT protmode now 0x%x"
	    , ic->ic_sta_assoc
	    , ic->ic_ht_sta_assoc
	    , ic->ic_ht40_sta_assoc
	    , (ic->ic_flags_ext & IEEE80211_FEXT_NONHT_PR) ?
		 ", non-HT sta present" : ""
	    , ic->ic_curhtprotmode);
	ieee80211_beacon_notify(ic, IEEE80211_BEACON_HTINFO);
}

/*
 * Calculate HT protection mode from current
 * state and handle updates.
 */
static void
htinfo_update(struct ieee80211com *ic)
{
	uint8_t protmode;

	if (ic->ic_flags_ext & IEEE80211_FEXT_NONHT_PR) {
		protmode = IEEE80211_HTINFO_OPMODE_PROTOPT
		         | IEEE80211_HTINFO_NONHT_PRESENT;
	} else if (ic->ic_sta_assoc != ic->ic_ht_sta_assoc) {
		protmode = IEEE80211_HTINFO_OPMODE_MIXED
		         | IEEE80211_HTINFO_NONHT_PRESENT;
	} else if (IEEE80211_IS_CHAN_HT40(ic->ic_bsschan) && 
	    ic->ic_sta_assoc != ic->ic_ht40_sta_assoc) {
		protmode = IEEE80211_HTINFO_OPMODE_HT20PR;
	} else {
		protmode = IEEE80211_HTINFO_OPMODE_PURE;
	}
	if (protmode != ic->ic_curhtprotmode) {
		ic->ic_curhtprotmode = protmode;
		htinfo_notify(ic);
	}
}

/*
 * Handle an HT station joining a BSS.
 */
void
ieee80211_ht_node_join(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_LOCK_ASSERT(ic);

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		ic->ic_ht_sta_assoc++;
		if (ni->ni_chw == 40)
			ic->ic_ht40_sta_assoc++;
	}
	htinfo_update(ic);
}

/*
 * Handle an HT station leaving a BSS.
 */
void
ieee80211_ht_node_leave(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_LOCK_ASSERT(ic);

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		ic->ic_ht_sta_assoc--;
		if (ni->ni_chw == 40)
			ic->ic_ht40_sta_assoc--;
	}
	htinfo_update(ic);
}

/*
 * Public version of htinfo_update; used for processing
 * beacon frames from overlapping bss in hostap_recv_mgmt.
 */
void
ieee80211_htinfo_update(struct ieee80211com *ic, int protmode)
{
	if (protmode != ic->ic_curhtprotmode) {
		ic->ic_curhtprotmode = protmode;
		htinfo_notify(ic);
	}
}

/*
 * Time out presence of an overlapping bss with non-HT
 * stations.  When operating in hostap mode we listen for
 * beacons from other stations and if we identify a non-HT
 * station is present we update the opmode field of the
 * HTINFO ie.  To identify when all non-HT stations are
 * gone we time out this condition.
 */
void
ieee80211_ht_timeout(struct ieee80211com *ic)
{
	IEEE80211_LOCK_ASSERT(ic);

	if ((ic->ic_flags_ext & IEEE80211_FEXT_NONHT_PR) &&
	    time_after(ticks, ic->ic_lastnonht + IEEE80211_NONHT_PRESENT_AGE)) {
#if 0
		IEEE80211_NOTE(ic, IEEE80211_MSG_11N, ni,
		    "%s", "time out non-HT STA present on channel");
#endif
		ic->ic_flags_ext &= ~IEEE80211_FEXT_NONHT_PR;
		htinfo_update(ic);
	}
}

/* unalligned little endian access */     
#define LE_READ_2(p)					\
	((uint16_t)					\
	 ((((const uint8_t *)(p))[0]      ) |		\
	  (((const uint8_t *)(p))[1] <<  8)))

/*
 * Process an 802.11n HT capabilities ie.
 */
void
ieee80211_parse_htcap(struct ieee80211_node *ni, const uint8_t *ie)
{
	struct ieee80211com *ic = ni->ni_ic;

	if (ie[0] == IEEE80211_ELEMID_VENDOR) {
		/*
		 * Station used Vendor OUI ie to associate;
		 * mark the node so when we respond we'll use
		 * the Vendor OUI's and not the standard ie's.
		 */
		ni->ni_flags |= IEEE80211_NODE_HTCOMPAT;
		ie += 4;
	} else
		ni->ni_flags &= ~IEEE80211_NODE_HTCOMPAT;

	ni->ni_htcap = LE_READ_2(ie +
		__offsetof(struct ieee80211_ie_htcap, hc_cap));
	ni->ni_htparam = ie[__offsetof(struct ieee80211_ie_htcap, hc_param)];
	/* XXX needed or will ieee80211_parse_htinfo always be called? */
	ni->ni_chw = (ni->ni_htcap & IEEE80211_HTCAP_CHWIDTH40) &&
		     (ic->ic_flags_ext & IEEE80211_FEXT_USEHT40) ? 40 : 20;
}

/*
 * Process an 802.11n HT info ie and update the node state.
 * Note that we handle use this information to identify the
 * correct channel (HT20, HT40+, HT40-, legacy).  The caller
 * is responsible for insuring any required channel change is
 * done (e.g. in sta mode when parsing the contents of a
 * beacon frame).
 */
void
ieee80211_parse_htinfo(struct ieee80211_node *ni, const uint8_t *ie)
{
	struct ieee80211com *ic = ni->ni_ic;
 	const struct ieee80211_ie_htinfo *htinfo;
	struct ieee80211_channel *c;
	uint16_t w;
	int htflags, chanflags;

	if (ie[0] == IEEE80211_ELEMID_VENDOR)
		ie += 4;
 	htinfo = (const struct ieee80211_ie_htinfo *) ie;
	ni->ni_htctlchan = htinfo->hi_ctrlchannel;
	ni->ni_ht2ndchan = SM(htinfo->hi_byte1, IEEE80211_HTINFO_2NDCHAN);
	w = LE_READ_2(&htinfo->hi_byte2);
	ni->ni_htopmode = SM(w, IEEE80211_HTINFO_OPMODE);
	w = LE_READ_2(&htinfo->hi_byte45);
	ni->ni_htstbc = SM(w, IEEE80211_HTINFO_BASIC_STBCMCS);
	/*
	 * Handle 11n channel switch.  Use the received HT ie's to
	 * identify the right channel to use.  If we cannot locate it
	 * in the channel table then fallback to legacy operation.
	 */
	htflags = (ic->ic_flags_ext & IEEE80211_FEXT_HT) ?
	    IEEE80211_CHAN_HT20 : 0;
	/* NB: honor operating mode constraint */
	if ((htinfo->hi_byte1 & IEEE80211_HTINFO_TXWIDTH_2040) &&
	    (ic->ic_flags_ext & IEEE80211_FEXT_USEHT40)) {
		if (ni->ni_ht2ndchan == IEEE80211_HTINFO_2NDCHAN_ABOVE)
			htflags = IEEE80211_CHAN_HT40U;
		else if (ni->ni_ht2ndchan == IEEE80211_HTINFO_2NDCHAN_BELOW)
			htflags = IEEE80211_CHAN_HT40D;
	}
	chanflags = (ni->ni_chan->ic_flags &~ IEEE80211_CHAN_HT) | htflags;
	if (chanflags != ni->ni_chan->ic_flags) {
		c = ieee80211_find_channel(ic, ni->ni_chan->ic_freq, chanflags);
		if (c == NULL && htflags != IEEE80211_CHAN_HT20) {
			/*
			 * No HT40 channel entry in our table; fall back
			 * to HT20 operation.  This should not happen.
			 */
			c = findhtchan(ic, ni->ni_chan, IEEE80211_CHAN_HT20);
			IEEE80211_NOTE(ni->ni_ic,
			    IEEE80211_MSG_ASSOC | IEEE80211_MSG_11N, ni,
			    "no HT40 channel (freq %u), falling back to HT20",
			    ni->ni_chan->ic_freq);
			/* XXX stat */
		}
		if (c != NULL && c != ni->ni_chan) {
			IEEE80211_NOTE(ni->ni_ic,
			    IEEE80211_MSG_ASSOC | IEEE80211_MSG_11N, ni,
			    "switch station to HT%d channel %u/0x%x",
			    IEEE80211_IS_CHAN_HT40(c) ? 40 : 20,
			    c->ic_freq, c->ic_flags);
			ni->ni_chan = c;
		}
		/* NB: caller responsible for forcing any channel change */
	}
	/* update node's tx channel width */
	ni->ni_chw = IEEE80211_IS_CHAN_HT40(ni->ni_chan)? 40 : 20;
}

/*
 * Install received HT rate set by parsing the HT cap ie.
 */
int
ieee80211_setup_htrates(struct ieee80211_node *ni, const uint8_t *ie, int flags)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_ie_htcap *htcap;
	struct ieee80211_htrateset *rs;
	int i;

	rs = &ni->ni_htrates;
	memset(rs, 0, sizeof(*rs));
	if (ie != NULL) {
		if (ie[0] == IEEE80211_ELEMID_VENDOR)
			ie += 4;
		htcap = (const struct ieee80211_ie_htcap *) ie;
		for (i = 0; i < IEEE80211_HTRATE_MAXSIZE; i++) {
			if (isclr(htcap->hc_mcsset, i))
				continue;
			if (rs->rs_nrates == IEEE80211_HTRATE_MAXSIZE) {
				IEEE80211_NOTE(ic,
				    IEEE80211_MSG_XRATE | IEEE80211_MSG_11N, ni,
				    "WARNING, HT rate set too large; only "
				    "using %u rates", IEEE80211_HTRATE_MAXSIZE);
				ic->ic_stats.is_rx_rstoobig++;
				break;
			}
			rs->rs_rates[rs->rs_nrates++] = i;
		}
	}
	return ieee80211_fix_rate(ni, (struct ieee80211_rateset *) rs, flags);
}

/*
 * Mark rates in a node's HT rate set as basic according
 * to the information in the supplied HT info ie.
 */
void
ieee80211_setup_basic_htrates(struct ieee80211_node *ni, const uint8_t *ie)
{
	const struct ieee80211_ie_htinfo *htinfo;
	struct ieee80211_htrateset *rs;
	int i, j;

	if (ie[0] == IEEE80211_ELEMID_VENDOR)
		ie += 4;
	htinfo = (const struct ieee80211_ie_htinfo *) ie;
	rs = &ni->ni_htrates;
	if (rs->rs_nrates == 0) {
		IEEE80211_NOTE(ni->ni_ic,
		    IEEE80211_MSG_XRATE | IEEE80211_MSG_11N, ni,
		    "%s", "WARNING, empty HT rate set");
		return;
	}
	for (i = 0; i < IEEE80211_HTRATE_MAXSIZE; i++) {
		if (isclr(htinfo->hi_basicmcsset, i))
			continue;
		for (j = 0; j < rs->rs_nrates; j++)
			if ((rs->rs_rates[j] & IEEE80211_RATE_VAL) == i)
				rs->rs_rates[j] |= IEEE80211_RATE_BASIC;
	}
}

static void
addba_timeout(void *arg)
{
	struct ieee80211_tx_ampdu *tap = arg;

	/* XXX ? */
	tap->txa_flags &= ~IEEE80211_AGGR_XCHGPEND;
	tap->txa_attempts++;
}

static void
addba_start_timeout(struct ieee80211_tx_ampdu *tap)
{
	/* XXX use CALLOUT_PENDING instead? */
	callout_reset(&tap->txa_timer, IEEE80211_AGGR_TIMEOUT,
	    addba_timeout, tap);
	tap->txa_flags |= IEEE80211_AGGR_XCHGPEND;
	tap->txa_lastrequest = ticks;
}

static void
addba_stop_timeout(struct ieee80211_tx_ampdu *tap)
{
	/* XXX use CALLOUT_PENDING instead? */
	if (tap->txa_flags & IEEE80211_AGGR_XCHGPEND) {
		callout_stop(&tap->txa_timer);
		tap->txa_flags &= ~IEEE80211_AGGR_XCHGPEND;
	}
}

/*
 * Default method for requesting A-MPDU tx aggregation.
 * We setup the specified state block and start a timer
 * to wait for an ADDBA response frame.
 */
static int
ieee80211_addba_request(struct ieee80211_node *ni,
	struct ieee80211_tx_ampdu *tap,
	int dialogtoken, int baparamset, int batimeout)
{
	int bufsiz;

	/* XXX locking */
	tap->txa_token = dialogtoken;
	tap->txa_flags |= IEEE80211_AGGR_IMMEDIATE;
	tap->txa_start = tap->txa_seqstart = 0;
	bufsiz = MS(baparamset, IEEE80211_BAPS_BUFSIZ);
	tap->txa_wnd = (bufsiz == 0) ?
	    IEEE80211_AGGR_BAWMAX : min(bufsiz, IEEE80211_AGGR_BAWMAX);
	addba_start_timeout(tap);
	return 1;
}

/*
 * Default method for processing an A-MPDU tx aggregation
 * response.  We shutdown any pending timer and update the
 * state block according to the reply.
 */
static int
ieee80211_addba_response(struct ieee80211_node *ni,
	struct ieee80211_tx_ampdu *tap,
	int status, int baparamset, int batimeout)
{
	int bufsiz;

	/* XXX locking */
	addba_stop_timeout(tap);
	if (status == IEEE80211_STATUS_SUCCESS) {
		bufsiz = MS(baparamset, IEEE80211_BAPS_BUFSIZ);
		/* XXX override our request? */
		tap->txa_wnd = (bufsiz == 0) ?
		    IEEE80211_AGGR_BAWMAX : min(bufsiz, IEEE80211_AGGR_BAWMAX);
		tap->txa_flags |= IEEE80211_AGGR_RUNNING;
	} else {
		/* mark tid so we don't try again */
		tap->txa_flags |= IEEE80211_AGGR_NAK;
	}
	return 1;
}

/*
 * Default method for stopping A-MPDU tx aggregation.
 * Any timer is cleared and we drain any pending frames.
 */
static void
ieee80211_addba_stop(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	/* XXX locking */
	addba_stop_timeout(tap);
	if (tap->txa_flags & IEEE80211_AGGR_RUNNING) {
		/* clear aggregation queue */
		ieee80211_drain_ifq(&tap->txa_q);
		tap->txa_flags &= ~IEEE80211_AGGR_RUNNING;
	}
	tap->txa_attempts = 0;
}

/*
 * Process a received action frame using the default aggregation
 * policy.  We intercept ADDBA-related frames and use them to
 * update our aggregation state.  All other frames are passed up
 * for processing by ieee80211_recv_action.
 */
static void
ieee80211_aggr_recv_action(struct ieee80211_node *ni,
	const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_action *ia;
	struct ieee80211_rx_ampdu *rap;
	struct ieee80211_tx_ampdu *tap;
	uint8_t dialogtoken;
	uint16_t baparamset, batimeout, baseqctl, code;
	uint16_t args[4];
	int tid, ac, bufsiz;

	ia = (const struct ieee80211_action *) frm;
	switch (ia->ia_category) {
	case IEEE80211_ACTION_CAT_BA:
		switch (ia->ia_action) {
		case IEEE80211_ACTION_BA_ADDBA_REQUEST:
			dialogtoken = frm[2];
			baparamset = LE_READ_2(frm+3);
			batimeout = LE_READ_2(frm+5);
			baseqctl = LE_READ_2(frm+7);

			tid = MS(baparamset, IEEE80211_BAPS_TID);
			bufsiz = MS(baparamset, IEEE80211_BAPS_BUFSIZ);

			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
			    "recv ADDBA request: dialogtoken %u "
			    "baparamset 0x%x (tid %d bufsiz %d) batimeout %d "
			    "baseqctl %d:%d",
			    dialogtoken, baparamset, tid, bufsiz, batimeout,
			    MS(baseqctl, IEEE80211_BASEQ_START),
			    MS(baseqctl, IEEE80211_BASEQ_FRAG));

			rap = &ni->ni_rx_ampdu[tid];

			/* Send ADDBA response */
			args[0] = dialogtoken;
			/*
			 * NB: We ack only if the sta associated with HT and
			 * the ap is configured to do AMPDU rx (the latter
			 * violates the 11n spec and is mostly for testing).
			 */
			if ((ni->ni_flags & IEEE80211_NODE_AMPDU_RX) &&
			    (ic->ic_flags_ext & IEEE80211_FEXT_AMPDU_RX)) {
				ampdu_rx_start(rap, bufsiz,
				    MS(baseqctl, IEEE80211_BASEQ_START));

				args[1] = IEEE80211_STATUS_SUCCESS;
			} else {
				IEEE80211_NOTE(ic,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N,
				    ni, "reject ADDBA request: %s",
				    ni->ni_flags & IEEE80211_NODE_AMPDU_RX ?
				       "administratively disabled" :
				       "not negotiated for station");
				ic->ic_stats.is_addba_reject++;
				args[1] = IEEE80211_STATUS_UNSPECIFIED;
			}
			/* XXX honor rap flags? */
			args[2] = IEEE80211_BAPS_POLICY_IMMEDIATE
				| SM(tid, IEEE80211_BAPS_TID)
				| SM(rap->rxa_wnd, IEEE80211_BAPS_BUFSIZ)
				;
			args[3] = 0;
			ic->ic_send_action(ni, IEEE80211_ACTION_CAT_BA,
				IEEE80211_ACTION_BA_ADDBA_RESPONSE, args);
			return;

		case IEEE80211_ACTION_BA_ADDBA_RESPONSE:
			dialogtoken = frm[2];
			code = LE_READ_2(frm+3);
			baparamset = LE_READ_2(frm+5);
			tid = MS(baparamset, IEEE80211_BAPS_TID);
			bufsiz = MS(baparamset, IEEE80211_BAPS_BUFSIZ);
			batimeout = LE_READ_2(frm+7);

			ac = TID_TO_WME_AC(tid);
			tap = &ni->ni_tx_ampdu[ac];
			if ((tap->txa_flags & IEEE80211_AGGR_XCHGPEND) == 0) {
				IEEE80211_DISCARD_MAC(ic,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N,
				    ni->ni_macaddr, "ADDBA response",
				    "no pending ADDBA, tid %d dialogtoken %u "
				    "code %d", tid, dialogtoken, code);
				ic->ic_stats.is_addba_norequest++;
				return;
			}
			if (dialogtoken != tap->txa_token) {
				IEEE80211_DISCARD_MAC(ic,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N,
				    ni->ni_macaddr, "ADDBA response",
				    "dialogtoken mismatch: waiting for %d, "
				    "received %d, tid %d code %d",
				    tap->txa_token, dialogtoken, tid, code);
				ic->ic_stats.is_addba_badtoken++;
				return;
			}

			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
			    "recv ADDBA response: dialogtoken %u code %d "
			    "baparamset 0x%x (tid %d bufsiz %d) batimeout %d",
			    dialogtoken, code, baparamset, tid, bufsiz,
			    batimeout);
			ic->ic_addba_response(ni, tap,
				code, baparamset, batimeout);
			return;

		case IEEE80211_ACTION_BA_DELBA:
			baparamset = LE_READ_2(frm+2);
			code = LE_READ_2(frm+4);

			tid = MS(baparamset, IEEE80211_DELBAPS_TID);

			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
			    "recv DELBA: baparamset 0x%x (tid %d initiator %d) "
			    "code %d", baparamset, tid,
			    MS(baparamset, IEEE80211_DELBAPS_INIT), code);

			if ((baparamset & IEEE80211_DELBAPS_INIT) == 0) {
				ac = TID_TO_WME_AC(tid);
				tap = &ni->ni_tx_ampdu[ac];
				ic->ic_addba_stop(ni, tap);
			} else {
				rap = &ni->ni_rx_ampdu[tid];
				ampdu_rx_stop(rap);
			}
			return;
		}
		break;
	}
	ieee80211_recv_action(ni, frm, efrm);
}

/*
 * Process a received 802.11n action frame.
 * Aggregation-related frames are assumed to be handled
 * already; we handle any other frames we can, otherwise
 * complain about being unsupported (with debugging).
 */
void
ieee80211_recv_action(struct ieee80211_node *ni,
	const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_action *ia;
	int chw;

	ia = (const struct ieee80211_action *) frm;
	switch (ia->ia_category) {
	case IEEE80211_ACTION_CAT_BA:
		IEEE80211_NOTE(ic,
		    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
		    "%s: BA action %d not implemented", __func__,
		    ia->ia_action);
		ic->ic_stats.is_rx_mgtdiscard++;
		break;
	case IEEE80211_ACTION_CAT_HT:
		switch (ia->ia_action) {
		case IEEE80211_ACTION_HT_TXCHWIDTH:
			chw = frm[2] == IEEE80211_A_HT_TXCHWIDTH_2040 ? 40 : 20;
			if (chw != ni->ni_chw) {
				ni->ni_chw = chw;
				ni->ni_flags |= IEEE80211_NODE_CHWUPDATE;
			}
			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
		            "%s: HT txchwidth, width %d (%s)",
			    __func__, chw,
			    ni->ni_flags & IEEE80211_NODE_CHWUPDATE ?
				"new" : "no change");
			break;
		case IEEE80211_ACTION_HT_MIMOPWRSAVE:
			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
		            "%s: HT MIMO PS", __func__);
			break;
		default:
			IEEE80211_NOTE(ic,
			   IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
		           "%s: HT action %d not implemented", __func__,
			   ia->ia_action);
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
		break;
	default:
		IEEE80211_NOTE(ic,
		    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
		    "%s: category %d not implemented", __func__,
		    ia->ia_category);
		ic->ic_stats.is_rx_mgtdiscard++;
		break;
	}
}

/*
 * Transmit processing.
 */

/*
 * Request A-MPDU tx aggregation.  Setup local state and
 * issue an ADDBA request.  BA use will only happen after
 * the other end replies with ADDBA response.
 */
int
ieee80211_ampdu_request(struct ieee80211_node *ni,
	struct ieee80211_tx_ampdu *tap)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t args[4];
	int tid, dialogtoken;
	static int tokens = 0;	/* XXX */

	/* XXX locking */
	if ((tap->txa_flags & IEEE80211_AGGR_SETUP) == 0) {
		/* do deferred setup of state */
		IEEE80211_TAPQ_INIT(tap);
		callout_init(&tap->txa_timer, CALLOUT_MPSAFE);
		tap->txa_flags |= IEEE80211_AGGR_SETUP;
	}
	if (tap->txa_attempts >= IEEE80211_AGGR_MAXTRIES &&
	    (ticks - tap->txa_lastrequest) < IEEE80211_AGGR_MINRETRY) {
		/*
		 * Don't retry too often; IEEE80211_AGGR_MINRETRY
		 * defines the minimum interval we'll retry after
		 * IEEE80211_AGGR_MAXTRIES failed attempts to
		 * negotiate use.
		 */
		return 0;
	}
	/* XXX hack for not doing proper locking */
	tap->txa_flags &= ~IEEE80211_AGGR_NAK;

	dialogtoken = (tokens+1) % 63;		/* XXX */

	tid = WME_AC_TO_TID(tap->txa_ac);
	args[0] = dialogtoken;
	args[1]	= IEEE80211_BAPS_POLICY_IMMEDIATE
		| SM(tid, IEEE80211_BAPS_TID)
		| SM(IEEE80211_AGGR_BAWMAX, IEEE80211_BAPS_BUFSIZ)
		;
	args[2] = 0;	/* batimeout */
	args[3] = SM(0, IEEE80211_BASEQ_START)
		| SM(0, IEEE80211_BASEQ_FRAG)
		;
	/* NB: do first so there's no race against reply */
	if (!ic->ic_addba_request(ni, tap, dialogtoken, args[1], args[2])) {
		/* unable to setup state, don't make request */
		IEEE80211_NOTE(ni->ni_ic, IEEE80211_MSG_11N,
		    ni, "%s: could not setup BA stream for AC %d",
		    __func__, tap->txa_ac);
		/* defer next try so we don't slam the driver with requests */
		tap->txa_attempts = IEEE80211_AGGR_MAXTRIES;
		tap->txa_lastrequest = ticks;
		return 0;
	}
	tokens = dialogtoken;			/* allocate token */
	return ic->ic_send_action(ni, IEEE80211_ACTION_CAT_BA,
		IEEE80211_ACTION_BA_ADDBA_REQUEST, args);
}

/*
 * Terminate an AMPDU tx stream.  State is reclaimed
 * and the peer notified with a DelBA Action frame.
 */
void
ieee80211_ampdu_stop(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t args[4];

	/* XXX locking */
	if (IEEE80211_AMPDU_RUNNING(tap)) {
		IEEE80211_NOTE(ic, IEEE80211_MSG_ACTION | IEEE80211_MSG_11N,
		    ni, "%s: stop BA stream for AC %d", __func__, tap->txa_ac);
		ic->ic_stats.is_ampdu_stop++;

		ic->ic_addba_stop(ni, tap);
		args[0] = WME_AC_TO_TID(tap->txa_ac);
		args[1] = IEEE80211_DELBAPS_INIT;
		args[2] = 1;				/* XXX reason code */
		ieee80211_send_action(ni, IEEE80211_ACTION_CAT_BA,
			IEEE80211_ACTION_BA_DELBA, args);
	} else {
		IEEE80211_NOTE(ic, IEEE80211_MSG_ACTION | IEEE80211_MSG_11N,
		    ni, "%s: BA stream for AC %d not running",
		    __func__, tap->txa_ac);
		ic->ic_stats.is_ampdu_stop_failed++;
	}
}

/*
 * Transmit a BAR frame to the specified node.  The
 * BAR contents are drawn from the supplied aggregation
 * state associated with the node.
 */
int
ieee80211_send_bar(struct ieee80211_node *ni,
	const struct ieee80211_tx_ampdu *tap)
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame_min *wh;
	struct mbuf *m;
	uint8_t *frm;
	uint16_t barctl, barseqctl;
	int tid, ret;

	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
		ic->ic_headroom + sizeof(struct ieee80211_frame_min),
		sizeof(struct ieee80211_ba_request)
	);
	if (m == NULL)
		senderr(ENOMEM, is_tx_nobuf);

	wh = mtod(m, struct ieee80211_frame_min *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 |
		IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_BAR;
	wh->i_fc[1] = 0;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);

	tid = WME_AC_TO_TID(tap->txa_ac);
	barctl 	= (tap->txa_flags & IEEE80211_AGGR_IMMEDIATE ?
			IEEE80211_BAPS_POLICY_IMMEDIATE :
			IEEE80211_BAPS_POLICY_DELAYED)
		| SM(tid, IEEE80211_BAPS_TID)
		| SM(tap->txa_wnd, IEEE80211_BAPS_BUFSIZ)
		;
	barseqctl = SM(tap->txa_start, IEEE80211_BASEQ_START)
		| SM(0, IEEE80211_BASEQ_FRAG)
		;
	ADDSHORT(frm, barctl);
	ADDSHORT(frm, barseqctl);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);

	IEEE80211_NODE_STAT(ni, tx_mgmt);	/* XXX tx_ctl? */

	IEEE80211_NOTE(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
	    ni, "send bar frame (tid %u start %u) on channel %u",
	    tid, tap->txa_start, ieee80211_chan2ieee(ic, ic->ic_curchan));

	m->m_pkthdr.rcvif = (void *)ni;
	IF_ENQUEUE(&ic->ic_mgtq, m);		/* cheat */
	if_start(ifp);

	return 0;
bad:
	ieee80211_free_node(ni);
	return ret;
#undef ADDSHORT
#undef senderr
}

/*
 * Send an action management frame.  The arguments are stuff
 * into a frame without inspection; the caller is assumed to
 * prepare them carefully (e.g. based on the aggregation state).
 */
int
ieee80211_send_action(struct ieee80211_node *ni,
	int category, int action, uint16_t args[4])
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m;
	uint8_t *frm;
	uint16_t baparamset;
	int ret;

	KASSERT(ni != NULL, ("null node"));

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n",
		__func__, __LINE__,
		ni, ether_sprintf(ni->ni_macaddr),
		ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
		ic->ic_headroom + sizeof(struct ieee80211_frame),
		  sizeof(uint16_t)	/* action+category */
		/* XXX may action payload */
		+ sizeof(struct ieee80211_action_ba_addbaresponse)
	);
	if (m == NULL)
		senderr(ENOMEM, is_tx_nobuf);

	*frm++ = category;
	*frm++ = action;
	switch (category) {
	case IEEE80211_ACTION_CAT_BA:
		switch (action) {
		case IEEE80211_ACTION_BA_ADDBA_REQUEST:
			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
			    "send ADDBA request: dialogtoken %d "
			    "baparamset 0x%x (tid %d) batimeout 0x%x baseqctl 0x%x",
			    args[0], args[1], MS(args[1], IEEE80211_BAPS_TID),
			    args[2], args[3]);

			*frm++ = args[0];	/* dialog token */
			ADDSHORT(frm, args[1]);	/* baparamset */
			ADDSHORT(frm, args[2]);	/* batimeout */
			ADDSHORT(frm, args[3]);	/* baseqctl */
			break;
		case IEEE80211_ACTION_BA_ADDBA_RESPONSE:
			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
			    "send ADDBA response: dialogtoken %d status %d "
			    "baparamset 0x%x (tid %d) batimeout %d",
			    args[0], args[1], args[2],
			    MS(args[2], IEEE80211_BAPS_TID), args[3]);

			*frm++ = args[0];	/* dialog token */
			ADDSHORT(frm, args[1]);	/* statuscode */
			ADDSHORT(frm, args[2]);	/* baparamset */
			ADDSHORT(frm, args[3]);	/* batimeout */
			break;
		case IEEE80211_ACTION_BA_DELBA:
			/* XXX */
			baparamset = SM(args[0], IEEE80211_DELBAPS_TID)
				   | SM(args[1], IEEE80211_DELBAPS_INIT)
				   ;
			ADDSHORT(frm, baparamset);
			ADDSHORT(frm, args[2]);	/* reason code */

			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
			    "send DELBA action: tid %d, initiator %d reason %d",
			    args[0], args[1], args[2]);
			break;
		default:
			goto badaction;
		}
		break;
	case IEEE80211_ACTION_CAT_HT:
		switch (action) {
		case IEEE80211_ACTION_HT_TXCHWIDTH:
			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N,
			    ni, "send HT txchwidth: width %d",
			    IEEE80211_IS_CHAN_HT40(ic->ic_bsschan) ? 40 : 20
			);
			*frm++ = IEEE80211_IS_CHAN_HT40(ic->ic_bsschan) ? 
				IEEE80211_A_HT_TXCHWIDTH_2040 :
				IEEE80211_A_HT_TXCHWIDTH_20;
			break;
		default:
			goto badaction;
		}
		break;
	default:
	badaction:
		IEEE80211_NOTE(ic,
		    IEEE80211_MSG_ACTION | IEEE80211_MSG_11N, ni,
		    "%s: unsupported category %d action %d", __func__,
		    category, action);
		senderr(EINVAL, is_tx_unknownmgt);
		/* NOTREACHED */
	}
	m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);

	ret = ieee80211_mgmt_output(ic, ni, m, IEEE80211_FC0_SUBTYPE_ACTION);
	if (ret != 0)
		goto bad;
	return 0;
bad:
	ieee80211_free_node(ni);
	return ret;
#undef ADDSHORT
#undef senderr
}

/*
 * Construct the MCS bit mask for inclusion
 * in an HT information element.
 */
static void 
ieee80211_set_htrates(uint8_t *frm, const struct ieee80211_htrateset *rs)
{
	int i;

	for (i = 0; i < rs->rs_nrates; i++) {
		int r = rs->rs_rates[i] & IEEE80211_RATE_VAL;
		if (r < IEEE80211_HTRATE_MAXSIZE) {	/* XXX? */
			/* NB: this assumes a particular implementation */
			setbit(frm, r);
		}
	}
}

/*
 * Add body of an HTCAP information element.
 */
static uint8_t *
ieee80211_add_htcap_body(uint8_t *frm, struct ieee80211_node *ni)
{
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t caps;
	int rxmax, density;

	/* HT capabilities */
	caps = ic->ic_htcaps & 0xffff;
	/*
	 * Note channel width depends on whether we are operating as
	 * a sta or not.  When operating as a sta we are generating
	 * a request based on our desired configuration.  Otherwise
	 * we are operational and the channel attributes identify
	 * how we've been setup (which might be different if a fixed
	 * channel is specified).
	 */
	if (ic->ic_opmode == IEEE80211_M_STA) {
		/* override 20/40 use based on config */
		if (ic->ic_flags_ext & IEEE80211_FEXT_USEHT40)
			caps |= IEEE80211_HTCAP_CHWIDTH40;
		else
			caps &= ~IEEE80211_HTCAP_CHWIDTH40;
		/* use advertised setting (XXX locally constraint) */
		rxmax = MS(ni->ni_htparam, IEEE80211_HTCAP_MAXRXAMPDU);
		density = MS(ni->ni_htparam, IEEE80211_HTCAP_MPDUDENSITY);
	} else {
		/* override 20/40 use based on current channel */
		if (IEEE80211_IS_CHAN_HT40(ic->ic_bsschan))
			caps |= IEEE80211_HTCAP_CHWIDTH40;
		else
			caps &= ~IEEE80211_HTCAP_CHWIDTH40;
		rxmax = ic->ic_ampdu_rxmax;
		density = ic->ic_ampdu_density;
	}
	/* adjust short GI based on channel and config */
	if ((ic->ic_flags_ext & IEEE80211_FEXT_SHORTGI20) == 0)
		caps &= ~IEEE80211_HTCAP_SHORTGI20;
	if ((ic->ic_flags_ext & IEEE80211_FEXT_SHORTGI40) == 0 ||
	    (caps & IEEE80211_HTCAP_CHWIDTH40) == 0)
		caps &= ~IEEE80211_HTCAP_SHORTGI40;
	ADDSHORT(frm, caps);

	/* HT parameters */
	*frm = SM(rxmax, IEEE80211_HTCAP_MAXRXAMPDU)
	     | SM(density, IEEE80211_HTCAP_MPDUDENSITY)
	     ;
	frm++;

	/* pre-zero remainder of ie */
	memset(frm, 0, sizeof(struct ieee80211_ie_htcap) - 
		__offsetof(struct ieee80211_ie_htcap, hc_mcsset));

	/* supported MCS set */
	/*
	 * XXX it would better to get the rate set from ni_htrates
	 * so we can restrict it but for sta mode ni_htrates isn't
	 * setup when we're called to form an AssocReq frame so for
	 * now we're restricted to the default HT rate set.
	 */
	ieee80211_set_htrates(frm, &ieee80211_rateset_11n);

	frm += sizeof(struct ieee80211_ie_htcap) -
		__offsetof(struct ieee80211_ie_htcap, hc_mcsset);
	return frm;
#undef ADDSHORT
}

/*
 * Add 802.11n HT capabilities information element
 */
uint8_t *
ieee80211_add_htcap(uint8_t *frm, struct ieee80211_node *ni)
{
	frm[0] = IEEE80211_ELEMID_HTCAP;
	frm[1] = sizeof(struct ieee80211_ie_htcap) - 2;
	return ieee80211_add_htcap_body(frm + 2, ni);
}

/*
 * Add Broadcom OUI wrapped standard HTCAP ie; this is
 * used for compatibility w/ pre-draft implementations.
 */
uint8_t *
ieee80211_add_htcap_vendor(uint8_t *frm, struct ieee80211_node *ni)
{
	frm[0] = IEEE80211_ELEMID_VENDOR;
	frm[1] = 4 + sizeof(struct ieee80211_ie_htcap) - 2;
	frm[2] = (BCM_OUI >> 0) & 0xff;
	frm[3] = (BCM_OUI >> 8) & 0xff;
	frm[4] = (BCM_OUI >> 16) & 0xff;
	frm[5] = BCM_OUI_HTCAP;
	return ieee80211_add_htcap_body(frm + 6, ni);
}

/*
 * Construct the MCS bit mask of basic rates
 * for inclusion in an HT information element.
 */
static void
ieee80211_set_basic_htrates(uint8_t *frm, const struct ieee80211_htrateset *rs)
{
	int i;

	for (i = 0; i < rs->rs_nrates; i++) {
		int r = rs->rs_rates[i] & IEEE80211_RATE_VAL;
		if ((rs->rs_rates[i] & IEEE80211_RATE_BASIC) &&
		    r < IEEE80211_HTRATE_MAXSIZE) {
			/* NB: this assumes a particular implementation */
			setbit(frm, r);
		}
	}
}

/*
 * Update the HTINFO ie for a beacon frame.
 */
void
ieee80211_ht_update_beacon(struct ieee80211com *ic,
	struct ieee80211_beacon_offsets *bo)
{
#define	PROTMODE	(IEEE80211_HTINFO_OPMODE|IEEE80211_HTINFO_NONHT_PRESENT)
	struct ieee80211_ie_htinfo *ht =
	   (struct ieee80211_ie_htinfo *) bo->bo_htinfo;

	/* XXX only update on channel change */
	ht->hi_ctrlchannel = ieee80211_chan2ieee(ic, ic->ic_bsschan);
	ht->hi_byte1 = IEEE80211_HTINFO_RIFSMODE_PROH;
	if (IEEE80211_IS_CHAN_HT40U(ic->ic_bsschan))
		ht->hi_byte1 |= IEEE80211_HTINFO_2NDCHAN_ABOVE;
	else if (IEEE80211_IS_CHAN_HT40D(ic->ic_bsschan))
		ht->hi_byte1 |= IEEE80211_HTINFO_2NDCHAN_BELOW;
	else
		ht->hi_byte1 |= IEEE80211_HTINFO_2NDCHAN_NONE;
	if (IEEE80211_IS_CHAN_HT40(ic->ic_bsschan))
		ht->hi_byte1 |= IEEE80211_HTINFO_TXWIDTH_2040;

	/* protection mode */
	ht->hi_byte2 = (ht->hi_byte2 &~ PROTMODE) | ic->ic_curhtprotmode;

	/* XXX propagate to vendor ie's */
#undef PROTMODE
}

/*
 * Add body of an HTINFO information element.
 *
 * NB: We don't use struct ieee80211_ie_htinfo because we can
 * be called to fillin both a standard ie and a compat ie that
 * has a vendor OUI at the front.
 */
static uint8_t *
ieee80211_add_htinfo_body(uint8_t *frm, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	/* pre-zero remainder of ie */
	memset(frm, 0, sizeof(struct ieee80211_ie_htinfo) - 2);

	/* primary/control channel center */
	*frm++ = ieee80211_chan2ieee(ic, ic->ic_bsschan);

	frm[0] = IEEE80211_HTINFO_RIFSMODE_PROH;
	if (IEEE80211_IS_CHAN_HT40U(ic->ic_bsschan))
		frm[0] |= IEEE80211_HTINFO_2NDCHAN_ABOVE;
	else if (IEEE80211_IS_CHAN_HT40D(ic->ic_bsschan))
		frm[0] |= IEEE80211_HTINFO_2NDCHAN_BELOW;
	else
		frm[0] |= IEEE80211_HTINFO_2NDCHAN_NONE;
	if (IEEE80211_IS_CHAN_HT40(ic->ic_bsschan))
		frm[0] |= IEEE80211_HTINFO_TXWIDTH_2040;

	frm[1] = ic->ic_curhtprotmode;

	frm += 5;

	/* basic MCS set */
	ieee80211_set_basic_htrates(frm, &ni->ni_htrates);
	frm += sizeof(struct ieee80211_ie_htinfo) -
		__offsetof(struct ieee80211_ie_htinfo, hi_basicmcsset);
	return frm;
}

/*
 * Add 802.11n HT information information element.
 */
uint8_t *
ieee80211_add_htinfo(uint8_t *frm, struct ieee80211_node *ni)
{
	frm[0] = IEEE80211_ELEMID_HTINFO;
	frm[1] = sizeof(struct ieee80211_ie_htinfo) - 2;
	return ieee80211_add_htinfo_body(frm + 2, ni);
}

/*
 * Add Broadcom OUI wrapped standard HTINFO ie; this is
 * used for compatibility w/ pre-draft implementations.
 */
uint8_t *
ieee80211_add_htinfo_vendor(uint8_t *frm, struct ieee80211_node *ni)
{
	frm[0] = IEEE80211_ELEMID_VENDOR;
	frm[1] = 4 + sizeof(struct ieee80211_ie_htinfo) - 2;
	frm[2] = (BCM_OUI >> 0) & 0xff;
	frm[3] = (BCM_OUI >> 8) & 0xff;
	frm[4] = (BCM_OUI >> 16) & 0xff;
	frm[5] = BCM_OUI_HTINFO;
	return ieee80211_add_htinfo_body(frm + 6, ni);
}
