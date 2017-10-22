/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD: release/7.0.0/sys/net80211/ieee80211_output.c 174393 2007-12-07 05:46:09Z kmacy $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/kernel.h>
#include <sys/endian.h>

#include <sys/socket.h>
 
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_vlan_var.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#define	ETHER_HEADER_COPY(dst, src) \
	memcpy(dst, src, sizeof(struct ether_header))

static struct mbuf *ieee80211_encap_fastframe(struct ieee80211com *ic,
	struct mbuf *m1, const struct ether_header *eh1,
	struct mbuf *m2, const struct ether_header *eh2);
static int ieee80211_fragment(struct ieee80211com *, struct mbuf *,
	u_int hdrsize, u_int ciphdrsize, u_int mtu);
static	void ieee80211_tx_mgt_cb(struct ieee80211_node *, void *, int);

#ifdef IEEE80211_DEBUG
/*
 * Decide if an outbound management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static __inline int
doprint(struct ieee80211com *ic, int subtype)
{
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		return (ic->ic_opmode == IEEE80211_M_IBSS);
	}
	return 1;
}
#endif

/*
 * Set the direction field and address fields of an outgoing
 * non-QoS frame.  Note this should be called early on in
 * constructing a frame as it sets i_fc[1]; other bits can
 * then be or'd in.
 */
static void
ieee80211_send_setup(struct ieee80211com *ic,
	struct ieee80211_node *ni,
	struct ieee80211_frame *wh,
	int type,
	const uint8_t sa[IEEE80211_ADDR_LEN],
	const uint8_t da[IEEE80211_ADDR_LEN],
	const uint8_t bssid[IEEE80211_ADDR_LEN])
{
#define	WH4(wh)	((struct ieee80211_frame_addr4 *)wh)

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | type;
	if ((type & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr2, sa);
			IEEE80211_ADDR_COPY(wh->i_addr3, da);
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, da);
			IEEE80211_ADDR_COPY(wh->i_addr2, sa);
			IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
			break;
		case IEEE80211_M_HOSTAP:
			wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
			IEEE80211_ADDR_COPY(wh->i_addr1, da);
			IEEE80211_ADDR_COPY(wh->i_addr2, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr3, sa);
			break;
		case IEEE80211_M_WDS:
			wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
			/* XXX cheat, bssid holds RA */
			IEEE80211_ADDR_COPY(wh->i_addr1, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
			IEEE80211_ADDR_COPY(wh->i_addr3, da);
			IEEE80211_ADDR_COPY(WH4(wh)->i_addr4, sa);
			break;
		case IEEE80211_M_MONITOR:	/* NB: to quiet compiler */
			break;
		}
	} else {
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, da);
		IEEE80211_ADDR_COPY(wh->i_addr2, sa);
		IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
	}
	*(uint16_t *)&wh->i_dur[0] = 0;
	/* NB: use non-QoS tid */
	*(uint16_t *)&wh->i_seq[0] =
	    htole16(ni->ni_txseqs[IEEE80211_NONQOS_TID] << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseqs[IEEE80211_NONQOS_TID]++;
#undef WH4
}

/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially free'ing up any associated storage).
 */
int
ieee80211_mgmt_output(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct mbuf *m, int type)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame *wh;

	KASSERT(ni != NULL, ("null node"));

	/*
	 * Yech, hack alert!  We want to pass the node down to the
	 * driver's start routine.  If we don't do so then the start
	 * routine must immediately look it up again and that can
	 * cause a lock order reversal if, for example, this frame
	 * is being sent because the station is being timedout and
	 * the frame being sent is a DEAUTH message.  We could stick
	 * this in an m_tag and tack that on to the mbuf.  However
	 * that's rather expensive to do for every frame so instead
	 * we stuff it in the rcvif field since outbound frames do
	 * not (presently) use this.
	 */
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	KASSERT(m->m_pkthdr.rcvif == NULL, ("rcvif not null"));
	m->m_pkthdr.rcvif = (void *)ni;

	wh = mtod(m, struct ieee80211_frame *);
	ieee80211_send_setup(ic, ni, wh, 
		IEEE80211_FC0_TYPE_MGT | type,
		ic->ic_myaddr, ni->ni_macaddr, ni->ni_bssid);
	if ((m->m_flags & M_LINK0) != 0 && ni->ni_challenge != NULL) {
		m->m_flags &= ~M_LINK0;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			"[%s] encrypting frame (%s)\n",
			ether_sprintf(wh->i_addr1), __func__);
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
	}
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		/* NB: force all management frames to the highest queue */
		M_WME_SETAC(m, WME_AC_VO);
	} else
		M_WME_SETAC(m, WME_AC_BE);
#ifdef IEEE80211_DEBUG
	/* avoid printing too many frames */
	if ((ieee80211_msg_debug(ic) && doprint(ic, type)) ||
	    ieee80211_msg_dumppkts(ic)) {
		printf("[%s] send %s on channel %u\n",
		    ether_sprintf(wh->i_addr1),
		    ieee80211_mgt_subtype_name[
			(type & IEEE80211_FC0_SUBTYPE_MASK) >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
		    ieee80211_chan2ieee(ic, ic->ic_curchan));
	}
#endif
	IEEE80211_NODE_STAT(ni, tx_mgmt);
	IF_ENQUEUE(&ic->ic_mgtq, m);
	if_start(ifp);
	ifp->if_opackets++;

	return 0;
}

/*
 * Raw packet transmit stub for legacy drivers.
 * Send the packet through the mgt q so we bypass
 * the normal encapsulation work.
 */
int
ieee80211_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;

	m->m_pkthdr.rcvif = (void *) ni;
	IF_ENQUEUE(&ic->ic_mgtq, m);
	if_start(ifp);
	ifp->if_opackets++;

	return 0;
}

/*
 * 802.11 output routine. This is (currently) used only to
 * connect bpf write calls to the 802.11 layer for injecting
 * raw 802.11 frames.  Note we locate the ieee80211com from
 * the ifnet using a spare field setup at attach time.  This
 * will go away when the virtual ap support comes in.
 */
int
ieee80211_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt0)
{
#define senderr(e) do { error = (e); goto bad;} while (0)
	struct ieee80211com *ic = ifp->if_llsoftc;	/* XXX */
	struct ieee80211_node *ni = NULL;
	struct ieee80211_frame *wh;
	int error;

	/*
	 * Hand to the 802.3 code if not tagged as
	 * a raw 802.11 frame.
	 */
	if (dst->sa_family != AF_IEEE80211)
		return ether_output(ifp, m, dst, rt0);
#ifdef MAC
	error = mac_check_ifnet_transmit(ifp, m);
	if (error)
		senderr(error);
#endif
	if (ifp->if_flags & IFF_MONITOR)
		senderr(ENETDOWN);
	if ((ifp->if_flags & IFF_UP) == 0)
		senderr(ENETDOWN);

	/* XXX bypass bridge, pfil, carp, etc. */

	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame_ack))
		senderr(EIO);	/* XXX */
	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0)
		senderr(EIO);	/* XXX */

	/* locate destination node */
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
	case IEEE80211_FC1_DIR_FROMDS:
		ni = ieee80211_find_txnode(ic, wh->i_addr1);
		break;
	case IEEE80211_FC1_DIR_TODS:
	case IEEE80211_FC1_DIR_DSTODS:
		if (m->m_pkthdr.len < sizeof(struct ieee80211_frame))
			senderr(EIO);	/* XXX */
		ni = ieee80211_find_txnode(ic, wh->i_addr3);
		break;
	default:
		senderr(EIO);	/* XXX */
	}
	if (ni == NULL) {
		/*
		 * Permit packets w/ bpf params through regardless
		 * (see below about sa_len).
		 */
		if (dst->sa_len == 0)
			senderr(EHOSTUNREACH);
		ni = ieee80211_ref_node(ic->ic_bss);
	}

	/* XXX ctrl frames should go through */
	if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
	    (m->m_flags & M_PWR_SAV) == 0) {
		/*
		 * Station in power save mode; pass the frame
		 * to the 802.11 layer and continue.  We'll get
		 * the frame back when the time is right.
		 */
		ieee80211_pwrsave(ni, m);
		error = 0;
		goto reclaim;
	}

	/* calculate priority so drivers can find the tx queue */
	/* XXX assumes an 802.3 frame */
	if (ieee80211_classify(ic, m, ni))
		senderr(EIO);		/* XXX */

	BPF_MTAP(ifp, m);
	/*
	 * NB: DLT_IEEE802_11_RADIO identifies the parameters are
	 * present by setting the sa_len field of the sockaddr (yes,
	 * this is a hack).
	 * NB: we assume sa_data is suitably aligned to cast.
	 */
	return ic->ic_raw_xmit(ni, m, (const struct ieee80211_bpf_params *)
		(dst->sa_len ? dst->sa_data : NULL));
bad:
	if (m != NULL)
		m_freem(m);
reclaim:
	if (ni != NULL)
		ieee80211_free_node(ni);
	return error;
#undef senderr
}

/*
 * Send a null data frame to the specified node.
 *
 * NB: the caller is assumed to have setup a node reference
 *     for use; this is necessary to deal with a race condition
 *     when probing for inactive stations.
 */
int
ieee80211_send_nulldata(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct mbuf *m;
	struct ieee80211_frame *wh;

	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL) {
		/* XXX debug msg */
		ieee80211_unref_node(&ni);
		ic->ic_stats.is_tx_nobuf++;
		return ENOMEM;
	}
	MH_ALIGN(m, sizeof(struct ieee80211_frame));
	m->m_pkthdr.rcvif = (void *) ni;

	wh = mtod(m, struct ieee80211_frame *);
	ieee80211_send_setup(ic, ni, wh,
		IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_NODATA,
		ic->ic_myaddr, ni->ni_macaddr, ni->ni_bssid);
	/* NB: power management bit is never sent by an AP */
	if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_WDS)
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
	m->m_len = m->m_pkthdr.len = sizeof(struct ieee80211_frame);
	M_WME_SETAC(m, WME_AC_BE);

	IEEE80211_NODE_STAT(ni, tx_data);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
	    "[%s] send null data frame on channel %u, pwr mgt %s\n",
	    ether_sprintf(ni->ni_macaddr),
	    ieee80211_chan2ieee(ic, ic->ic_curchan),
	    wh->i_fc[1] & IEEE80211_FC1_PWR_MGT ? "ena" : "dis");

	IF_ENQUEUE(&ic->ic_mgtq, m);		/* cheat */
	if_start(ifp);

	return 0;
}

/* 
 * Assign priority to a frame based on any vlan tag assigned
 * to the station and/or any Diffserv setting in an IP header.
 * Finally, if an ACM policy is setup (in station mode) it's
 * applied.
 */
int
ieee80211_classify(struct ieee80211com *ic, struct mbuf *m, struct ieee80211_node *ni)
{
	int v_wme_ac, d_wme_ac, ac;
#ifdef INET
	struct ether_header *eh;
#endif

	if ((ni->ni_flags & IEEE80211_NODE_QOS) == 0) {
		ac = WME_AC_BE;
		goto done;
	}

	/* 
	 * If node has a vlan tag then all traffic
	 * to it must have a matching tag.
	 */
	v_wme_ac = 0;
	if (ni->ni_vlan != 0) {
		 if ((m->m_flags & M_VLANTAG) == 0) {
			IEEE80211_NODE_STAT(ni, tx_novlantag);
			return 1;
		}
		if (EVL_VLANOFTAG(m->m_pkthdr.ether_vtag) !=
		    EVL_VLANOFTAG(ni->ni_vlan)) {
			IEEE80211_NODE_STAT(ni, tx_vlanmismatch);
			return 1;
		}
		/* map vlan priority to AC */
		v_wme_ac = TID_TO_WME_AC(EVL_PRIOFTAG(ni->ni_vlan));
	}

#ifdef INET
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		uint8_t tos;
		/*
		 * IP frame, map the DSCP bits from the TOS field.
		 */
		/* XXX m_copydata may be too slow for fast path */
		/* NB: ip header may not be in first mbuf */
		m_copydata(m, sizeof(struct ether_header) +
		    offsetof(struct ip, ip_tos), sizeof(tos), &tos);
		tos >>= 5;		/* NB: ECN + low 3 bits of DSCP */
		d_wme_ac = TID_TO_WME_AC(tos);
	} else {
#endif /* INET */
		d_wme_ac = WME_AC_BE;
#ifdef INET
	}
#endif
	/*
	 * Use highest priority AC.
	 */
	if (v_wme_ac > d_wme_ac)
		ac = v_wme_ac;
	else
		ac = d_wme_ac;

	/*
	 * Apply ACM policy.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA) {
		static const int acmap[4] = {
			WME_AC_BK,	/* WME_AC_BE */
			WME_AC_BK,	/* WME_AC_BK */
			WME_AC_BE,	/* WME_AC_VI */
			WME_AC_VI,	/* WME_AC_VO */
		};
		while (ac != WME_AC_BK &&
		    ic->ic_wme.wme_wmeBssChanParams.cap_wmeParams[ac].wmep_acm)
			ac = acmap[ac];
	}
done:
	M_WME_SETAC(m, ac);
	return 0;
}

/*
 * Insure there is sufficient contiguous space to encapsulate the
 * 802.11 data frame.  If room isn't already there, arrange for it.
 * Drivers and cipher modules assume we have done the necessary work
 * and fail rudely if they don't find the space they need.
 */
static struct mbuf *
ieee80211_mbuf_adjust(struct ieee80211com *ic, int hdrsize,
	struct ieee80211_key *key, struct mbuf *m)
{
#define	TO_BE_RECLAIMED	(sizeof(struct ether_header) - sizeof(struct llc))
	int needed_space = ic->ic_headroom + hdrsize;

	if (key != NULL) {
		/* XXX belongs in crypto code? */
		needed_space += key->wk_cipher->ic_header;
		/* XXX frags */
		/*
		 * When crypto is being done in the host we must insure
		 * the data are writable for the cipher routines; clone
		 * a writable mbuf chain.
		 * XXX handle SWMIC specially
		 */
		if (key->wk_flags & (IEEE80211_KEY_SWCRYPT|IEEE80211_KEY_SWMIC)) {
			m = m_unshare(m, M_NOWAIT);
			if (m == NULL) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
				    "%s: cannot get writable mbuf\n", __func__);
				ic->ic_stats.is_tx_nobuf++; /* XXX new stat */
				return NULL;
			}
		}
	}
	/*
	 * We know we are called just before stripping an Ethernet
	 * header and prepending an LLC header.  This means we know
	 * there will be
	 *	sizeof(struct ether_header) - sizeof(struct llc)
	 * bytes recovered to which we need additional space for the
	 * 802.11 header and any crypto header.
	 */
	/* XXX check trailing space and copy instead? */
	if (M_LEADINGSPACE(m) < needed_space - TO_BE_RECLAIMED) {
		struct mbuf *n = m_gethdr(M_NOWAIT, m->m_type);
		if (n == NULL) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
			    "%s: cannot expand storage\n", __func__);
			ic->ic_stats.is_tx_nobuf++;
			m_freem(m);
			return NULL;
		}
		KASSERT(needed_space <= MHLEN,
		    ("not enough room, need %u got %zu\n", needed_space, MHLEN));
		/*
		 * Setup new mbuf to have leading space to prepend the
		 * 802.11 header and any crypto header bits that are
		 * required (the latter are added when the driver calls
		 * back to ieee80211_crypto_encap to do crypto encapsulation).
		 */
		/* NB: must be first 'cuz it clobbers m_data */
		m_move_pkthdr(n, m);
		n->m_len = 0;			/* NB: m_gethdr does not set */
		n->m_data += needed_space;
		/*
		 * Pull up Ethernet header to create the expected layout.
		 * We could use m_pullup but that's overkill (i.e. we don't
		 * need the actual data) and it cannot fail so do it inline
		 * for speed.
		 */
		/* NB: struct ether_header is known to be contiguous */
		n->m_len += sizeof(struct ether_header);
		m->m_len -= sizeof(struct ether_header);
		m->m_data += sizeof(struct ether_header);
		/*
		 * Replace the head of the chain.
		 */
		n->m_next = m;
		m = n;
	}
	return m;
#undef TO_BE_RECLAIMED
}

/*
 * Return the transmit key to use in sending a unicast frame.
 * If a unicast key is set we use that.  When no unicast key is set
 * we fall back to the default transmit key.
 */ 
static __inline struct ieee80211_key *
ieee80211_crypto_getucastkey(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (IEEE80211_KEY_UNDEFINED(&ni->ni_ucastkey)) {
		if (ic->ic_def_txkey == IEEE80211_KEYIX_NONE ||
		    IEEE80211_KEY_UNDEFINED(&ic->ic_nw_keys[ic->ic_def_txkey]))
			return NULL;
		return &ic->ic_nw_keys[ic->ic_def_txkey];
	} else {
		return &ni->ni_ucastkey;
	}
}

/*
 * Return the transmit key to use in sending a multicast frame.
 * Multicast traffic always uses the group key which is installed as
 * the default tx key.
 */ 
static __inline struct ieee80211_key *
ieee80211_crypto_getmcastkey(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ic->ic_def_txkey == IEEE80211_KEYIX_NONE ||
	    IEEE80211_KEY_UNDEFINED(&ic->ic_nw_keys[ic->ic_def_txkey]))
		return NULL;
	return &ic->ic_nw_keys[ic->ic_def_txkey];
}

/*
 * Encapsulate an outbound data frame.  The mbuf chain is updated.
 * If an error is encountered NULL is returned.  The caller is required
 * to provide a node reference and pullup the ethernet header in the
 * first mbuf.
 */
struct mbuf *
ieee80211_encap(struct ieee80211com *ic, struct mbuf *m,
	struct ieee80211_node *ni)
{
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	struct llc *llc;
	int hdrsize, datalen, addqos, txfrag, isff;

	/*
	 * Copy existing Ethernet header to a safe place.  The
	 * rest of the code assumes it's ok to strip it when
	 * reorganizing state for the final encapsulation.
	 */
	KASSERT(m->m_len >= sizeof(eh), ("no ethernet header!"));
	memcpy(&eh, mtod(m, caddr_t), sizeof(struct ether_header));

	/*
	 * Insure space for additional headers.  First identify
	 * transmit key to use in calculating any buffer adjustments
	 * required.  This is also used below to do privacy
	 * encapsulation work.  Then calculate the 802.11 header
	 * size and any padding required by the driver.
	 *
	 * Note key may be NULL if we fall back to the default
	 * transmit key and that is not set.  In that case the
	 * buffer may not be expanded as needed by the cipher
	 * routines, but they will/should discard it.
	 */
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		if (ic->ic_opmode == IEEE80211_M_STA ||
		    !IEEE80211_IS_MULTICAST(eh.ether_dhost))
			key = ieee80211_crypto_getucastkey(ic, ni);
		else
			key = ieee80211_crypto_getmcastkey(ic, ni);
		if (key == NULL && eh.ether_type != htons(ETHERTYPE_PAE)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,
			    "[%s] no default transmit key (%s) deftxkey %u\n",
			    ether_sprintf(eh.ether_dhost), __func__,
			    ic->ic_def_txkey);
			ic->ic_stats.is_tx_nodefkey++;
			goto bad;
		}
	} else
		key = NULL;
	/* XXX 4-address format */
	/*
	 * XXX Some ap's don't handle QoS-encapsulated EAPOL
	 * frames so suppress use.  This may be an issue if other
	 * ap's require all data frames to be QoS-encapsulated
	 * once negotiated in which case we'll need to make this
	 * configurable.
	 */
	addqos = (ni->ni_flags & (IEEE80211_NODE_QOS|IEEE80211_NODE_HT)) &&
		 eh.ether_type != htons(ETHERTYPE_PAE);
	if (addqos)
		hdrsize = sizeof(struct ieee80211_qosframe);
	else
		hdrsize = sizeof(struct ieee80211_frame);
	if (ic->ic_flags & IEEE80211_F_DATAPAD)
		hdrsize = roundup(hdrsize, sizeof(uint32_t));

	if ((isff = m->m_flags & M_FF) != 0) {
		struct mbuf *m2;
		struct ether_header eh2;

		/*
		 * Fast frame encapsulation.  There must be two packets
		 * chained with m_nextpkt.  We do header adjustment for
		 * each, add the tunnel encapsulation, and then concatenate
		 * the mbuf chains to form a single frame for transmission.
		 */
		m2 = m->m_nextpkt;
		if (m2 == NULL) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
				"%s: only one frame\n", __func__);
			goto bad;
		}
		m->m_nextpkt = NULL;
		/*
		 * Include fast frame headers in adjusting header
		 * layout; this allocates space according to what
		 * ieee80211_encap_fastframe will do.
		 */
		m = ieee80211_mbuf_adjust(ic,
			hdrsize + sizeof(struct llc) + sizeof(uint32_t) + 2 +
			    sizeof(struct ether_header),
			key, m);
		if (m == NULL) {
			/* NB: ieee80211_mbuf_adjust handles msgs+statistics */
			m_freem(m2);
			goto bad;
		}
		/*
		 * Copy second frame's Ethernet header out of line
		 * and adjust for encapsulation headers.  Note that
		 * we make room for padding in case there isn't room
		 * at the end of first frame.
		 */
		KASSERT(m2->m_len >= sizeof(eh2), ("no ethernet header!"));
		memcpy(&eh2, mtod(m2, caddr_t), sizeof(struct ether_header));
		m2 = ieee80211_mbuf_adjust(ic,
			ATH_FF_MAX_HDR_PAD + sizeof(struct ether_header),
			NULL, m2);
		if (m2 == NULL) {
			/* NB: ieee80211_mbuf_adjust handles msgs+statistics */
			goto bad;
		}
		m = ieee80211_encap_fastframe(ic, m, &eh, m2, &eh2);
		if (m == NULL)
			goto bad;
	} else {
		/*
		 * Normal frame.
		 */
		m = ieee80211_mbuf_adjust(ic, hdrsize, key, m);
		if (m == NULL) {
			/* NB: ieee80211_mbuf_adjust handles msgs+statistics */
			goto bad;
		}
		/* NB: this could be optimized 'cuz of ieee80211_mbuf_adjust */
		m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
		llc = mtod(m, struct llc *);
		llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		llc->llc_snap.org_code[0] = 0;
		llc->llc_snap.org_code[1] = 0;
		llc->llc_snap.org_code[2] = 0;
		llc->llc_snap.ether_type = eh.ether_type;
	}
	datalen = m->m_pkthdr.len;		/* NB: w/o 802.11 header */

	M_PREPEND(m, hdrsize, M_DONTWAIT);
	if (m == NULL) {
		ic->ic_stats.is_tx_nobuf++;
		goto bad;
	}
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(uint16_t *)wh->i_dur = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		/*
		 * NB: always use the bssid from ic_bss as the
		 *     neighbor's may be stale after an ibss merge
		 */
		IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_bss->ni_bssid);
		break;
	case IEEE80211_M_HOSTAP:
		wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
		break;
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_WDS:
		goto bad;
	}
	if (m->m_flags & M_MORE_DATA)
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
	if (addqos) {
		struct ieee80211_qosframe *qwh =
			(struct ieee80211_qosframe *) wh;
		int ac, tid;

		ac = M_WME_GETAC(m);
		/* map from access class/queue to 11e header priorty value */
		tid = WME_AC_TO_TID(ac);
		qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
		/*
		 * Check if A-MPDU tx aggregation is setup or if we
		 * should try to enable it.  The sta must be associated
		 * with HT and A-MPDU enabled for use.  On the first
		 * frame that goes out We issue an ADDBA request and
		 * wait for a reply.  The frame being encapsulated
		 * will go out w/o using A-MPDU, or possibly it might
		 * be collected by the driver and held/retransmit.
		 * ieee80211_ampdu_request handles staggering requests
		 * in case the receiver NAK's us or we are otherwise
		 * unable to establish a BA stream.
		 */
		if ((ni->ni_flags & IEEE80211_NODE_AMPDU_TX) &&
		    (ic->ic_flags_ext & IEEE80211_FEXT_AMPDU_TX)) {
			struct ieee80211_tx_ampdu *tap = &ni->ni_tx_ampdu[ac];

			if (IEEE80211_AMPDU_RUNNING(tap)) {
				/*
				 * Operational, mark frame for aggregation.
				 */
				qwh->i_qos[0] |= IEEE80211_QOS_ACKPOLICY_BA;
			} else if (!IEEE80211_AMPDU_REQUESTED(tap)) {
				/*
				 * Not negotiated yet, request service.
				 */
				ieee80211_ampdu_request(ni, tap);
			}
		}
		/* XXX works even when BA marked above */
		if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy)
			qwh->i_qos[0] |= IEEE80211_QOS_ACKPOLICY_NOACK;
		qwh->i_qos[1] = 0;
		qwh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;

		*(uint16_t *)wh->i_seq =
		    htole16(ni->ni_txseqs[tid] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[tid]++;
	} else {
		*(uint16_t *)wh->i_seq =
		    htole16(ni->ni_txseqs[IEEE80211_NONQOS_TID] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[IEEE80211_NONQOS_TID]++;
	}
	/* check if xmit fragmentation is required */
	txfrag = (m->m_pkthdr.len > ic->ic_fragthreshold &&
	    !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (ic->ic_caps & IEEE80211_C_TXFRAG) &&
	    !isff);		/* NB: don't fragment ff's */
	if (key != NULL) {
		/*
		 * IEEE 802.1X: send EAPOL frames always in the clear.
		 * WPA/WPA2: encrypt EAPOL keys when pairwise keys are set.
		 */
		if (eh.ether_type != htons(ETHERTYPE_PAE) ||
		    ((ic->ic_flags & IEEE80211_F_WPA) &&
		     (ic->ic_opmode == IEEE80211_M_STA ?
		      !IEEE80211_KEY_UNDEFINED(key) :
		      !IEEE80211_KEY_UNDEFINED(&ni->ni_ucastkey)))) {
			wh->i_fc[1] |= IEEE80211_FC1_WEP;
			if (!ieee80211_crypto_enmic(ic, key, m, txfrag)) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
				    "[%s] enmic failed, discard frame\n",
				    ether_sprintf(eh.ether_dhost));
				ic->ic_stats.is_crypto_enmicfail++;
				goto bad;
			}
		}
	}
	/*
	 * NB: frag flags may leak from above; they should only
	 *     be set on return to the caller if we fragment at
	 *     the 802.11 layer.
	 */
	m->m_flags &= ~(M_FRAG | M_FIRSTFRAG);
	if (txfrag && !ieee80211_fragment(ic, m, hdrsize,
	    key != NULL ? key->wk_cipher->ic_header : 0, ic->ic_fragthreshold))
		goto bad;

	IEEE80211_NODE_STAT(ni, tx_data);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		IEEE80211_NODE_STAT(ni, tx_mcast);
	else
		IEEE80211_NODE_STAT(ni, tx_ucast);
	IEEE80211_NODE_STAT_ADD(ni, tx_bytes, datalen);

	return m;
bad:
	if (m != NULL)
		m_freem(m);
	return NULL;
}

/*
 * Do Ethernet-LLC encapsulation for each payload in a fast frame
 * tunnel encapsulation.  The frame is assumed to have an Ethernet
 * header at the front that must be stripped before prepending the
 * LLC followed by the Ethernet header passed in (with an Ethernet
 * type that specifies the payload size).
 */
static struct mbuf *
ieee80211_encap1(struct ieee80211com *ic, struct mbuf *m,
	const struct ether_header *eh)
{
	struct llc *llc;
	uint16_t payload;

	/* XXX optimize by combining m_adj+M_PREPEND */
	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh->ether_type;
	payload = m->m_pkthdr.len;		/* NB: w/o Ethernet header */

	M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
	if (m == NULL) {		/* XXX cannot happen */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
			"%s: no space for ether_header\n", __func__);
		ic->ic_stats.is_tx_nobuf++;
		return NULL;
	}
	ETHER_HEADER_COPY(mtod(m, void *), eh);
	mtod(m, struct ether_header *)->ether_type = htons(payload);
	return m;
}

/*
 * Do fast frame tunnel encapsulation.  The two frames and
 * Ethernet headers are supplied.  The caller is assumed to
 * have arrange for space in the mbuf chains for encapsulating
 * headers (to avoid major mbuf fragmentation).
 *
 * The encapsulated frame is returned or NULL if there is a
 * problem (should not happen).
 */
static struct mbuf *
ieee80211_encap_fastframe(struct ieee80211com *ic,
	struct mbuf *m1, const struct ether_header *eh1,
	struct mbuf *m2, const struct ether_header *eh2)
{
	struct llc *llc;
	struct mbuf *m;
	int pad;

	/*
	 * First, each frame gets a standard encapsulation.
	 */
	m1 = ieee80211_encap1(ic, m1, eh1);
	if (m1 == NULL) {
		m_freem(m2);
		return NULL;
	}
	m2 = ieee80211_encap1(ic, m2, eh2);
	if (m2 == NULL) {
		m_freem(m1);
		return NULL;
	}

	/*
	 * Pad leading frame to a 4-byte boundary.  If there
	 * is space at the end of the first frame, put it
	 * there; otherwise prepend to the front of the second
	 * frame.  We know doing the second will always work
	 * because we reserve space above.  We prefer appending
	 * as this typically has better DMA alignment properties.
	 */
	for (m = m1; m->m_next != NULL; m = m->m_next)
		;
	pad = roundup2(m1->m_pkthdr.len, 4) - m1->m_pkthdr.len;
	if (pad) {
		if (M_TRAILINGSPACE(m) < pad) {		/* prepend to second */
			m2->m_data -= pad;
			m2->m_len += pad;
			m2->m_pkthdr.len += pad;
		} else {				/* append to first */
			m->m_len += pad;
			m1->m_pkthdr.len += pad;
		}
	}

	/*
	 * Now, stick 'em together and prepend the tunnel headers;
	 * first the Atheros tunnel header (all zero for now) and
	 * then a special fast frame LLC.
	 *
	 * XXX optimize by prepending together
	 */
	m->m_next = m2;			/* NB: last mbuf from above */
	m1->m_pkthdr.len += m2->m_pkthdr.len;
	M_PREPEND(m1, sizeof(uint32_t)+2, M_DONTWAIT);
	if (m1 == NULL) {		/* XXX cannot happen */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
			"%s: no space for tunnel header\n", __func__);
		ic->ic_stats.is_tx_nobuf++;
		return NULL;
	}
	memset(mtod(m1, void *), 0, sizeof(uint32_t)+2);

	M_PREPEND(m1, sizeof(struct llc), M_DONTWAIT);
	if (m1 == NULL) {		/* XXX cannot happen */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
			"%s: no space for llc header\n", __func__);
		ic->ic_stats.is_tx_nobuf++;
		return NULL;
	}
	llc = mtod(m1, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = ATH_FF_SNAP_ORGCODE_0;
	llc->llc_snap.org_code[1] = ATH_FF_SNAP_ORGCODE_1;
	llc->llc_snap.org_code[2] = ATH_FF_SNAP_ORGCODE_2;
	llc->llc_snap.ether_type = htons(ATH_FF_ETH_TYPE);

	ic->ic_stats.is_ff_encap++;

	return m1;
}

/*
 * Fragment the frame according to the specified mtu.
 * The size of the 802.11 header (w/o padding) is provided
 * so we don't need to recalculate it.  We create a new
 * mbuf for each fragment and chain it through m_nextpkt;
 * we might be able to optimize this by reusing the original
 * packet's mbufs but that is significantly more complicated.
 */
static int
ieee80211_fragment(struct ieee80211com *ic, struct mbuf *m0,
	u_int hdrsize, u_int ciphdrsize, u_int mtu)
{
	struct ieee80211_frame *wh, *whf;
	struct mbuf *m, *prev, *next;
	u_int totalhdrsize, fragno, fragsize, off, remainder, payload;

	KASSERT(m0->m_nextpkt == NULL, ("mbuf already chained?"));
	KASSERT(m0->m_pkthdr.len > mtu,
		("pktlen %u mtu %u", m0->m_pkthdr.len, mtu));

	wh = mtod(m0, struct ieee80211_frame *);
	/* NB: mark the first frag; it will be propagated below */
	wh->i_fc[1] |= IEEE80211_FC1_MORE_FRAG;
	totalhdrsize = hdrsize + ciphdrsize;
	fragno = 1;
	off = mtu - ciphdrsize;
	remainder = m0->m_pkthdr.len - off;
	prev = m0;
	do {
		fragsize = totalhdrsize + remainder;
		if (fragsize > mtu)
			fragsize = mtu;
		KASSERT(fragsize < MCLBYTES,
			("fragment size %u too big!", fragsize));
		if (fragsize > MHLEN)
			m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		else
			m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			goto bad;
		/* leave room to prepend any cipher header */
		m_align(m, fragsize - ciphdrsize);

		/*
		 * Form the header in the fragment.  Note that since
		 * we mark the first fragment with the MORE_FRAG bit
		 * it automatically is propagated to each fragment; we
		 * need only clear it on the last fragment (done below).
		 */
		whf = mtod(m, struct ieee80211_frame *);
		memcpy(whf, wh, hdrsize);
		*(uint16_t *)&whf->i_seq[0] |= htole16(
			(fragno & IEEE80211_SEQ_FRAG_MASK) <<
				IEEE80211_SEQ_FRAG_SHIFT);
		fragno++;

		payload = fragsize - totalhdrsize;
		/* NB: destination is known to be contiguous */
		m_copydata(m0, off, payload, mtod(m, uint8_t *) + hdrsize);
		m->m_len = hdrsize + payload;
		m->m_pkthdr.len = hdrsize + payload;
		m->m_flags |= M_FRAG;

		/* chain up the fragment */
		prev->m_nextpkt = m;
		prev = m;

		/* deduct fragment just formed */
		remainder -= payload;
		off += payload;
	} while (remainder != 0);
	whf->i_fc[1] &= ~IEEE80211_FC1_MORE_FRAG;

	/* strip first mbuf now that everything has been copied */
	m_adj(m0, -(m0->m_pkthdr.len - (mtu - ciphdrsize)));
	m0->m_flags |= M_FIRSTFRAG | M_FRAG;

	ic->ic_stats.is_tx_fragframes++;
	ic->ic_stats.is_tx_frags += fragno-1;

	return 1;
bad:
	/* reclaim fragments but leave original frame for caller to free */
	for (m = m0->m_nextpkt; m != NULL; m = next) {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;		/* XXX paranoid */
		m_freem(m);
	}
	m0->m_nextpkt = NULL;
	return 0;
}

/*
 * Add a supported rates element id to a frame.
 */
static uint8_t *
ieee80211_add_rates(uint8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add an extended supported rates element id to a frame.
 */
static uint8_t *
ieee80211_add_xrates(uint8_t *frm, const struct ieee80211_rateset *rs)
{
	/*
	 * Add an extended supported rates element if operating in 11g mode.
	 */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		int nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}
	return frm;
}

/* 
 * Add an ssid elemet to a frame.
 */
static uint8_t *
ieee80211_add_ssid(uint8_t *frm, const uint8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add an erp element to a frame.
 */
static uint8_t *
ieee80211_add_erp(uint8_t *frm, struct ieee80211com *ic)
{
	uint8_t erp;

	*frm++ = IEEE80211_ELEMID_ERP;
	*frm++ = 1;
	erp = 0;
	if (ic->ic_nonerpsta != 0)
		erp |= IEEE80211_ERP_NON_ERP_PRESENT;
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		erp |= IEEE80211_ERP_USE_PROTECTION;
	if (ic->ic_flags & IEEE80211_F_USEBARKER)
		erp |= IEEE80211_ERP_LONG_PREAMBLE;
	*frm++ = erp;
	return frm;
}

static uint8_t *
ieee80211_setup_wpa_ie(struct ieee80211com *ic, uint8_t *ie)
{
#define	WPA_OUI_BYTES		0x00, 0x50, 0xf2
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)
	static const uint8_t oui[4] = { WPA_OUI_BYTES, WPA_OUI_TYPE };
	static const uint8_t cipher_suite[][4] = {
		{ WPA_OUI_BYTES, WPA_CSE_WEP40 },	/* NB: 40-bit */
		{ WPA_OUI_BYTES, WPA_CSE_TKIP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX WRAP */
		{ WPA_OUI_BYTES, WPA_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ WPA_OUI_BYTES, WPA_CSE_NULL },
	};
	static const uint8_t wep104_suite[4] =
		{ WPA_OUI_BYTES, WPA_CSE_WEP104 };
	static const uint8_t key_mgt_unspec[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_UNSPEC };
	static const uint8_t key_mgt_psk[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	uint8_t *frm = ie;
	uint8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* WPA OUI */
	frm += sizeof(oui);
	ADDSHORT(frm, WPA_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_TKIP)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
	}

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	if (rsn->rsn_caps != 0 && rsn->rsn_caps != RSN_CAP_PREAUTH)
		ADDSHORT(frm, rsn->rsn_caps);

	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
		("WPA IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSHORT
#undef ADDSELECTOR
#undef WPA_OUI_BYTES
}

static uint8_t *
ieee80211_setup_rsn_ie(struct ieee80211com *ic, uint8_t *ie)
{
#define	RSN_OUI_BYTES		0x00, 0x0f, 0xac
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)
	static const uint8_t cipher_suite[][4] = {
		{ RSN_OUI_BYTES, RSN_CSE_WEP40 },	/* NB: 40-bit */
		{ RSN_OUI_BYTES, RSN_CSE_TKIP },
		{ RSN_OUI_BYTES, RSN_CSE_WRAP },
		{ RSN_OUI_BYTES, RSN_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ RSN_OUI_BYTES, RSN_CSE_NULL },
	};
	static const uint8_t wep104_suite[4] =
		{ RSN_OUI_BYTES, RSN_CSE_WEP104 };
	static const uint8_t key_mgt_unspec[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_UNSPEC };
	static const uint8_t key_mgt_psk[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	uint8_t *frm = ie;
	uint8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_RSN;
	*frm++ = 0;				/* length filled in below */
	ADDSHORT(frm, RSN_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_TKIP)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
	}

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	ADDSHORT(frm, rsn->rsn_caps);
	/* XXX PMKID */

	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
		("RSN IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSELECTOR
#undef ADDSHORT
#undef RSN_OUI_BYTES
}

/*
 * Add a WPA/RSN element to a frame.
 */
static uint8_t *
ieee80211_add_wpa(uint8_t *frm, struct ieee80211com *ic)
{

	KASSERT(ic->ic_flags & IEEE80211_F_WPA, ("no WPA/RSN!"));
	if (ic->ic_flags & IEEE80211_F_WPA2)
		frm = ieee80211_setup_rsn_ie(ic, frm);
	if (ic->ic_flags & IEEE80211_F_WPA1)
		frm = ieee80211_setup_wpa_ie(ic, frm);
	return frm;
}

#define	WME_OUI_BYTES		0x00, 0x50, 0xf2
/*
 * Add a WME information element to a frame.
 */
static uint8_t *
ieee80211_add_wme_info(uint8_t *frm, struct ieee80211_wme_state *wme)
{
	static const struct ieee80211_wme_info info = {
		.wme_id		= IEEE80211_ELEMID_VENDOR,
		.wme_len	= sizeof(struct ieee80211_wme_info) - 2,
		.wme_oui	= { WME_OUI_BYTES },
		.wme_type	= WME_OUI_TYPE,
		.wme_subtype	= WME_INFO_OUI_SUBTYPE,
		.wme_version	= WME_VERSION,
		.wme_info	= 0,
	};
	memcpy(frm, &info, sizeof(info));
	return frm + sizeof(info); 
}

/*
 * Add a WME parameters element to a frame.
 */
static uint8_t *
ieee80211_add_wme_param(uint8_t *frm, struct ieee80211_wme_state *wme)
{
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
	/* NB: this works 'cuz a param has an info at the front */
	static const struct ieee80211_wme_info param = {
		.wme_id		= IEEE80211_ELEMID_VENDOR,
		.wme_len	= sizeof(struct ieee80211_wme_param) - 2,
		.wme_oui	= { WME_OUI_BYTES },
		.wme_type	= WME_OUI_TYPE,
		.wme_subtype	= WME_PARAM_OUI_SUBTYPE,
		.wme_version	= WME_VERSION,
	};
	int i;

	memcpy(frm, &param, sizeof(param));
	frm += __offsetof(struct ieee80211_wme_info, wme_info);
	*frm++ = wme->wme_bssChanParams.cap_info;	/* AC info */
	*frm++ = 0;					/* reserved field */
	for (i = 0; i < WME_NUM_AC; i++) {
		const struct wmeParams *ac =
		       &wme->wme_bssChanParams.cap_wmeParams[i];
		*frm++ = SM(i, WME_PARAM_ACI)
		       | SM(ac->wmep_acm, WME_PARAM_ACM)
		       | SM(ac->wmep_aifsn, WME_PARAM_AIFSN)
		       ;
		*frm++ = SM(ac->wmep_logcwmax, WME_PARAM_LOGCWMAX)
		       | SM(ac->wmep_logcwmin, WME_PARAM_LOGCWMIN)
		       ;
		ADDSHORT(frm, ac->wmep_txopLimit);
	}
	return frm;
#undef SM
#undef ADDSHORT
}
#undef WME_OUI_BYTES

#define	ATH_OUI_BYTES		0x00, 0x03, 0x7f
/*
 * Add a WME information element to a frame.
 */
static uint8_t *
ieee80211_add_ath(uint8_t *frm, uint8_t caps, uint16_t defkeyix)
{
	static const struct ieee80211_ath_ie info = {
		.ath_id		= IEEE80211_ELEMID_VENDOR,
		.ath_len	= sizeof(struct ieee80211_ath_ie) - 2,
		.ath_oui	= { ATH_OUI_BYTES },
		.ath_oui_type	= ATH_OUI_TYPE,
		.ath_oui_subtype= ATH_OUI_SUBTYPE,
		.ath_version	= ATH_OUI_VERSION,
	};
	struct ieee80211_ath_ie *ath = (struct ieee80211_ath_ie *) frm;

	memcpy(frm, &info, sizeof(info));
	ath->ath_capability = caps;
	ath->ath_defkeyix[0] = (defkeyix & 0xff);
	ath->ath_defkeyix[1] = ((defkeyix >> 8) & 0xff);
	return frm + sizeof(info); 
}
#undef ATH_OUI_BYTES

/*
 * Send a probe request frame with the specified ssid
 * and any optional information element data.
 */
int
ieee80211_send_probereq(struct ieee80211_node *ni,
	const uint8_t sa[IEEE80211_ADDR_LEN],
	const uint8_t da[IEEE80211_ADDR_LEN],
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t *ssid, size_t ssidlen,
	const void *optie, size_t optielen)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame *wh;
	const struct ieee80211_rateset *rs;
	struct mbuf *m;
	uint8_t *frm;

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

	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 *	[tlv] user-specified ie's
	 */
	m = ieee80211_getmgtframe(&frm,
		 ic->ic_headroom + sizeof(struct ieee80211_frame),
		 2 + IEEE80211_NWID_LEN
	       + 2 + IEEE80211_RATE_SIZE
	       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
	       + (optie != NULL ? optielen : 0)
	);
	if (m == NULL) {
		ic->ic_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}

	frm = ieee80211_add_ssid(frm, ssid, ssidlen);
	rs = ieee80211_get_suprates(ic, ic->ic_curchan);
	frm = ieee80211_add_rates(frm, rs);
	frm = ieee80211_add_xrates(frm, rs);

	if (optie != NULL) {
		memcpy(frm, optie, optielen);
		frm += optielen;
	}
	m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);

	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	KASSERT(m->m_pkthdr.rcvif == NULL, ("rcvif not null"));
	m->m_pkthdr.rcvif = (void *)ni;

	wh = mtod(m, struct ieee80211_frame *);
	ieee80211_send_setup(ic, ni, wh,
		IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_REQ,
		sa, da, bssid);
	/* XXX power management? */

	IEEE80211_NODE_STAT(ni, tx_probereq);
	IEEE80211_NODE_STAT(ni, tx_mgmt);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
	    "[%s] send probe req on channel %u\n",
	    ether_sprintf(wh->i_addr1),
	    ieee80211_chan2ieee(ic, ic->ic_curchan));

	IF_ENQUEUE(&ic->ic_mgtq, m);
	if_start(ic->ic_ifp);
	return 0;
}

/*
 * Calculate capability information for mgt frames.
 */
static uint16_t
getcapinfo(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	uint16_t capinfo;

	KASSERT(ic->ic_opmode != IEEE80211_M_STA, ("station mode"));

	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		capinfo = IEEE80211_CAPINFO_ESS;
	else if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = 0;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	return capinfo;
}

/*
 * Send a management frame.  The node is for the destination (or ic_bss
 * when in station mode).  Nodes other than ic_bss have their reference
 * count bumped to reflect our use for an indeterminant time.
 */
int
ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
	int type, int arg)
{
#define	HTFLAGS (IEEE80211_NODE_HT | IEEE80211_NODE_HTCOMPAT)
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
	const struct ieee80211_rateset *rs;
	struct mbuf *m;
	uint8_t *frm;
	uint16_t capinfo;
	int has_challenge, is_shared_key, ret, status;

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

	switch (type) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		/*
		 * probe response frame format
		 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] cabability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] parameter set (FH/DS)
		 *	[tlv] parameter set (IBSS)
		 *	[tlv] extended rate phy (ERP)
		 *	[tlv] extended supported rates
		 *	[tlv] WPA
		 *	[tlv] WME (optional)
		 *	[tlv] HT capabilities
		 *	[tlv] HT information
		 *	[tlv] Vendor OUI HT capabilities (optional)
		 *	[tlv] Vendor OUI HT information (optional)
		 *	[tlv] Atheros capabilities
		 */
		m = ieee80211_getmgtframe(&frm,
			 ic->ic_headroom + sizeof(struct ieee80211_frame),
			 8
		       + sizeof(uint16_t)
		       + sizeof(uint16_t)
		       + 2 + IEEE80211_NWID_LEN
		       + 2 + IEEE80211_RATE_SIZE
		       + 7	/* max(7,3) */
		       + 6
		       + 3
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		       /* XXX !WPA1+WPA2 fits w/o a cluster */
		       + (ic->ic_flags & IEEE80211_F_WPA ?
				2*sizeof(struct ieee80211_ie_wpa) : 0)
		       + sizeof(struct ieee80211_wme_param)
		       /* XXX check for cluster requirement */
		       + 2*sizeof(struct ieee80211_ie_htcap) + 4
		       + 2*sizeof(struct ieee80211_ie_htinfo) + 4
		       + sizeof(struct ieee80211_ath_ie)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		memset(frm, 0, 8);	/* timestamp should be filled later */
		frm += 8;
		*(uint16_t *)frm = htole16(ic->ic_bss->ni_intval);
		frm += 2;
		capinfo = getcapinfo(ic, ic->ic_curchan);
		*(uint16_t *)frm = htole16(capinfo);
		frm += 2;

		frm = ieee80211_add_ssid(frm, ic->ic_bss->ni_essid,
				ic->ic_bss->ni_esslen);
		rs = ieee80211_get_suprates(ic, ic->ic_curchan);
		frm = ieee80211_add_rates(frm, rs);

		if (IEEE80211_IS_CHAN_FHSS(ic->ic_curchan)) {
                        *frm++ = IEEE80211_ELEMID_FHPARMS;
                        *frm++ = 5;
                        *frm++ = ni->ni_fhdwell & 0x00ff;
                        *frm++ = (ni->ni_fhdwell >> 8) & 0x00ff;
                        *frm++ = IEEE80211_FH_CHANSET(
			    ieee80211_chan2ieee(ic, ic->ic_curchan));
                        *frm++ = IEEE80211_FH_CHANPAT(
			    ieee80211_chan2ieee(ic, ic->ic_curchan));
                        *frm++ = ni->ni_fhindex;
		} else {
			*frm++ = IEEE80211_ELEMID_DSPARMS;
			*frm++ = 1;
			*frm++ = ieee80211_chan2ieee(ic, ic->ic_curchan);
		}

		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			*frm++ = IEEE80211_ELEMID_IBSSPARMS;
			*frm++ = 2;
			*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
		}
		if (ic->ic_flags & IEEE80211_F_WPA)
			frm = ieee80211_add_wpa(frm, ic);
		if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan))
			frm = ieee80211_add_erp(frm, ic);
		frm = ieee80211_add_xrates(frm, rs);
		/*
		 * NB: legacy 11b clients do not get certain ie's.
		 *     The caller identifies such clients by passing
		 *     a token in arg to us.  Could expand this to be
		 *     any legacy client for stuff like HT ie's.
		 */
		if (IEEE80211_IS_CHAN_HT(ic->ic_curchan) &&
		    arg != IEEE80211_SEND_LEGACY_11B) {
			frm = ieee80211_add_htcap(frm, ni);
			frm = ieee80211_add_htinfo(frm, ni);
		}
		if (ic->ic_flags & IEEE80211_F_WME)
			frm = ieee80211_add_wme_param(frm, &ic->ic_wme);
		if (IEEE80211_IS_CHAN_HT(ic->ic_curchan) &&
		    (ic->ic_flags_ext & IEEE80211_FEXT_HTCOMPAT) &&
		    arg != IEEE80211_SEND_LEGACY_11B) {
			frm = ieee80211_add_htcap_vendor(frm, ni);
			frm = ieee80211_add_htinfo_vendor(frm, ni);
		}
		if (ni->ni_ath_ie != NULL)
			frm = ieee80211_add_ath(frm, ni->ni_ath_flags,
				ni->ni_ath_defkeyix);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH:
		status = arg >> 16;
		arg &= 0xffff;
		has_challenge = ((arg == IEEE80211_AUTH_SHARED_CHALLENGE ||
		    arg == IEEE80211_AUTH_SHARED_RESPONSE) &&
		    ni->ni_challenge != NULL);

		/*
		 * Deduce whether we're doing open authentication or
		 * shared key authentication.  We do the latter if
		 * we're in the middle of a shared key authentication
		 * handshake or if we're initiating an authentication
		 * request and configured to use shared key.
		 */
		is_shared_key = has_challenge ||
		     arg >= IEEE80211_AUTH_SHARED_RESPONSE ||
		     (arg == IEEE80211_AUTH_SHARED_REQUEST &&
		      ic->ic_bss->ni_authmode == IEEE80211_AUTH_SHARED);

		m = ieee80211_getmgtframe(&frm,
			  ic->ic_headroom + sizeof(struct ieee80211_frame),
			  3 * sizeof(uint16_t)
			+ (has_challenge && status == IEEE80211_STATUS_SUCCESS ?
				sizeof(uint16_t)+IEEE80211_CHALLENGE_LEN : 0)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		((uint16_t *)frm)[0] =
		    (is_shared_key) ? htole16(IEEE80211_AUTH_ALG_SHARED)
		                    : htole16(IEEE80211_AUTH_ALG_OPEN);
		((uint16_t *)frm)[1] = htole16(arg);	/* sequence number */
		((uint16_t *)frm)[2] = htole16(status);/* status */

		if (has_challenge && status == IEEE80211_STATUS_SUCCESS) {
			((uint16_t *)frm)[3] =
			    htole16((IEEE80211_CHALLENGE_LEN << 8) |
			    IEEE80211_ELEMID_CHALLENGE);
			memcpy(&((uint16_t *)frm)[4], ni->ni_challenge,
			    IEEE80211_CHALLENGE_LEN);
			m->m_pkthdr.len = m->m_len =
				4 * sizeof(uint16_t) + IEEE80211_CHALLENGE_LEN;
			if (arg == IEEE80211_AUTH_SHARED_RESPONSE) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				    "[%s] request encrypt frame (%s)\n",
				    ether_sprintf(ni->ni_macaddr), __func__);
				m->m_flags |= M_LINK0; /* WEP-encrypt, please */
			}
		} else
			m->m_pkthdr.len = m->m_len = 3 * sizeof(uint16_t);

		/* XXX not right for shared key */
		if (status == IEEE80211_STATUS_SUCCESS)
			IEEE80211_NODE_STAT(ni, tx_auth);
		else
			IEEE80211_NODE_STAT(ni, tx_auth_fail);

		if (ic->ic_opmode == IEEE80211_M_STA)
			ieee80211_add_callback(m, ieee80211_tx_mgt_cb,
				(void *) ic->ic_state);
		break;

	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			"[%s] send station deauthenticate (reason %d)\n",
			ether_sprintf(ni->ni_macaddr), arg);
		m = ieee80211_getmgtframe(&frm,
			ic->ic_headroom + sizeof(struct ieee80211_frame),
			sizeof(uint16_t));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);
		*(uint16_t *)frm = htole16(arg);	/* reason */
		m->m_pkthdr.len = m->m_len = sizeof(uint16_t);

		IEEE80211_NODE_STAT(ni, tx_deauth);
		IEEE80211_NODE_STAT_SET(ni, tx_deauth_code, arg);

		ieee80211_node_unauthorize(ni);		/* port closed */
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] listen interval
		 *	[6*] current AP address (reassoc only)
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *	[tlv] WME
		 *	[tlv] HT capabilities
		 *	[tlv] Vendor OUI HT capabilities (optional)
		 *	[tlv] Atheros capabilities (if negotiated)
		 *	[tlv] user-specified ie's
		 */
		m = ieee80211_getmgtframe(&frm,
			 ic->ic_headroom + sizeof(struct ieee80211_frame),
			 sizeof(uint16_t)
		       + sizeof(uint16_t)
		       + IEEE80211_ADDR_LEN
		       + 2 + IEEE80211_NWID_LEN
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		       + sizeof(struct ieee80211_wme_info)
		       + 2*sizeof(struct ieee80211_ie_htcap) + 4
		       + sizeof(struct ieee80211_ath_ie)
		       + (ic->ic_opt_ie != NULL ? ic->ic_opt_ie_len : 0)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		KASSERT(ic->ic_opmode == IEEE80211_M_STA,
		    ("wrong mode %u", ic->ic_opmode));
		capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		/*
		 * NB: Some 11a AP's reject the request when
		 *     short premable is set.
		 */
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
		    (ic->ic_caps & IEEE80211_C_SHSLOT))
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SPECTRUM_MGMT) &&
		    (ic->ic_flags & IEEE80211_F_DOTH))
			capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
		*(uint16_t *)frm = htole16(capinfo);
		frm += 2;

		KASSERT(ic->ic_bss->ni_intval != 0,
			("beacon interval is zero!"));
		*(uint16_t *)frm = htole16(howmany(ic->ic_lintval,
						   ic->ic_bss->ni_intval));
		frm += 2;

		if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
			IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
			frm += IEEE80211_ADDR_LEN;
		}

		frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		if ((ic->ic_flags_ext & IEEE80211_FEXT_HT) &&
		    ni->ni_htcap_ie != NULL &&
		    ni->ni_htcap_ie[0] == IEEE80211_ELEMID_HTCAP)
			frm = ieee80211_add_htcap(frm, ni);
		if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
			frm = ieee80211_add_wme_info(frm, &ic->ic_wme);
		if ((ic->ic_flags_ext & IEEE80211_FEXT_HT) &&
		    ni->ni_htcap_ie != NULL &&
		    ni->ni_htcap_ie[0] == IEEE80211_ELEMID_VENDOR)
			frm = ieee80211_add_htcap_vendor(frm, ni);
		if (IEEE80211_ATH_CAP(ic, ni, IEEE80211_F_ATHEROS))
			frm = ieee80211_add_ath(frm,
				IEEE80211_ATH_CAP(ic, ni, IEEE80211_F_ATHEROS),
				(ic->ic_flags & IEEE80211_F_WPA) == 0 &&
				ni->ni_authmode != IEEE80211_AUTH_8021X &&
				ic->ic_def_txkey != IEEE80211_KEYIX_NONE ?
				ic->ic_def_txkey : 0x7fff);
		if (ic->ic_opt_ie != NULL) {
			memcpy(frm, ic->ic_opt_ie, ic->ic_opt_ie_len);
			frm += ic->ic_opt_ie_len;
		}
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);

		ieee80211_add_callback(m, ieee80211_tx_mgt_cb,
			(void *) ic->ic_state);
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		/*
		 * asresp frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *	[tlv] WME (if enabled and STA enabled)
		 *	[tlv] HT capabilities (standard or vendor OUI)
		 *	[tlv] HT information (standard or vendor OUI)
		 *	[tlv] Atheros capabilities (if enabled and STA enabled)
		 */
		m = ieee80211_getmgtframe(&frm,
			 ic->ic_headroom + sizeof(struct ieee80211_frame),
			 sizeof(uint16_t)
		       + sizeof(uint16_t)
		       + sizeof(uint16_t)
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		       + sizeof(struct ieee80211_wme_param)
		       + sizeof(struct ieee80211_ie_htcap) + 4
		       + sizeof(struct ieee80211_ie_htinfo) + 4
		       + sizeof(struct ieee80211_ath_ie)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		capinfo = getcapinfo(ic, ic->ic_curchan);
		*(uint16_t *)frm = htole16(capinfo);
		frm += 2;

		*(uint16_t *)frm = htole16(arg);	/* status */
		frm += 2;

		if (arg == IEEE80211_STATUS_SUCCESS) {
			*(uint16_t *)frm = htole16(ni->ni_associd);
			IEEE80211_NODE_STAT(ni, tx_assoc);
		} else
			IEEE80211_NODE_STAT(ni, tx_assoc_fail);
		frm += 2;

		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		/* NB: respond according to what we received */
		if ((ni->ni_flags & HTFLAGS) == IEEE80211_NODE_HT) {
			frm = ieee80211_add_htcap(frm, ni);
			frm = ieee80211_add_htinfo(frm, ni);
		}
		if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
			frm = ieee80211_add_wme_param(frm, &ic->ic_wme);
		if ((ni->ni_flags & HTFLAGS) == HTFLAGS) {
			frm = ieee80211_add_htcap_vendor(frm, ni);
			frm = ieee80211_add_htinfo_vendor(frm, ni);
		}
		if (IEEE80211_ATH_CAP(ic, ni, IEEE80211_F_ATHEROS))
			frm = ieee80211_add_ath(frm,
				IEEE80211_ATH_CAP(ic, ni, IEEE80211_F_ATHEROS),
				ni->ni_ath_defkeyix);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			"[%s] send station disassociate (reason %d)\n",
			ether_sprintf(ni->ni_macaddr), arg);
		m = ieee80211_getmgtframe(&frm,
			ic->ic_headroom + sizeof(struct ieee80211_frame),
			sizeof(uint16_t));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);
		*(uint16_t *)frm = htole16(arg);	/* reason */
		m->m_pkthdr.len = m->m_len = sizeof(uint16_t);

		IEEE80211_NODE_STAT(ni, tx_disassoc);
		IEEE80211_NODE_STAT_SET(ni, tx_disassoc_code, arg);
		break;

	default:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"[%s] invalid mgmt frame type %u\n",
			ether_sprintf(ni->ni_macaddr), type);
		senderr(EINVAL, is_tx_unknownmgt);
		/* NOTREACHED */
	}

	ret = ieee80211_mgmt_output(ic, ni, m, type);
	if (ret != 0)
		goto bad;
	return 0;
bad:
	ieee80211_free_node(ni);
	return ret;
#undef senderr
#undef HTFLAGS
}

static void
ieee80211_tx_mgt_timeout(void *arg)
{
	struct ieee80211_node *ni = arg;
	struct ieee80211com *ic	= ni->ni_ic;

	if (ic->ic_state != IEEE80211_S_INIT &&
	    (ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		/*
		 * NB: it's safe to specify a timeout as the reason here;
		 *     it'll only be used in the right state.
		 */
		ieee80211_new_state(ic, IEEE80211_S_SCAN,
			IEEE80211_SCAN_FAIL_TIMEOUT);
	}
}

static void
ieee80211_tx_mgt_cb(struct ieee80211_node *ni, void *arg, int status)
{
	struct ieee80211com *ic = ni->ni_ic;
	enum ieee80211_state ostate = (enum ieee80211_state) arg;

	/*
	 * Frame transmit completed; arrange timer callback.  If
	 * transmit was successfuly we wait for response.  Otherwise
	 * we arrange an immediate callback instead of doing the
	 * callback directly since we don't know what state the driver
	 * is in (e.g. what locks it is holding).  This work should
	 * not be too time-critical and not happen too often so the
	 * added overhead is acceptable.
	 *
	 * XXX what happens if !acked but response shows up before callback?
	 */
	if (ic->ic_state == ostate)
		callout_reset(&ic->ic_mgtsend,
			status == 0 ? IEEE80211_TRANS_WAIT*hz : 0,
			ieee80211_tx_mgt_timeout, ni);
}

/*
 * Allocate a beacon frame and fillin the appropriate bits.
 */
struct mbuf *
ieee80211_beacon_alloc(struct ieee80211_node *ni,
	struct ieee80211_beacon_offsets *bo)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame *wh;
	struct mbuf *m;
	int pktlen;
	uint8_t *frm;
	uint16_t capinfo;
	struct ieee80211_rateset *rs;

	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[3] parameter set (DS)
	 *	[tlv] parameter set (IBSS/TIM)
	 *	[tlv] country code
	 *	[tlv] extended rate phy (ERP)
	 *	[tlv] extended supported rates
	 *	[tlv] WME parameters
	 *	[tlv] WPA/RSN parameters
	 *	[tlv] HT capabilities
	 *	[tlv] HT information
	 *	[tlv] Vendor OUI HT capabilities (optional)
	 *	[tlv] Vendor OUI HT information (optional)
	 * XXX Vendor-specific OIDs (e.g. Atheros)
	 * NB: we allocate the max space required for the TIM bitmap.
	 */
	rs = &ni->ni_rates;
	pktlen =   8					/* time stamp */
		 + sizeof(uint16_t)			/* beacon interval */
		 + sizeof(uint16_t)			/* capabilities */
		 + 2 + ni->ni_esslen			/* ssid */
	         + 2 + IEEE80211_RATE_SIZE		/* supported rates */
	         + 2 + 1				/* DS parameters */
		 + 2 + 4 + ic->ic_tim_len		/* DTIM/IBSSPARMS */
		 + sizeof(struct ieee80211_country_ie)	/* country code */
		 + 2 + 1				/* ERP */
	         + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		 + (ic->ic_caps & IEEE80211_C_WME ?	/* WME */
			sizeof(struct ieee80211_wme_param) : 0)
		 + (ic->ic_caps & IEEE80211_C_WPA ?	/* WPA 1+2 */
			2*sizeof(struct ieee80211_ie_wpa) : 0)
		 /* XXX conditional? */
		 + 4+2*sizeof(struct ieee80211_ie_htcap)/* HT caps */
		 + 4+2*sizeof(struct ieee80211_ie_htinfo)/* HT info */
		 ;
	m = ieee80211_getmgtframe(&frm,
		ic->ic_headroom + sizeof(struct ieee80211_frame), pktlen);
	if (m == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"%s: cannot get buf; size %u\n", __func__, pktlen);
		ic->ic_stats.is_tx_nobuf++;
		return NULL;
	}

	memset(bo, 0, sizeof(*bo));

	memset(frm, 0, 8);	/* XXX timestamp is set by hardware/driver */
	frm += 8;
	*(uint16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	capinfo = getcapinfo(ic, ni->ni_chan);
	bo->bo_caps = (uint16_t *)frm;
	*(uint16_t *)frm = htole16(capinfo);
	frm += 2;
	*frm++ = IEEE80211_ELEMID_SSID;
	if ((ic->ic_flags & IEEE80211_F_HIDESSID) == 0) {
		*frm++ = ni->ni_esslen;
		memcpy(frm, ni->ni_essid, ni->ni_esslen);
		frm += ni->ni_esslen;
	} else
		*frm++ = 0;
	frm = ieee80211_add_rates(frm, rs);
	if (!IEEE80211_IS_CHAN_FHSS(ic->ic_bsschan)) {
		*frm++ = IEEE80211_ELEMID_DSPARMS;
		*frm++ = 1;
		*frm++ = ieee80211_chan2ieee(ic, ic->ic_bsschan);
	}
	bo->bo_tim = frm;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
		bo->bo_tim_len = 0;
	} else if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		struct ieee80211_tim_ie *tie = (struct ieee80211_tim_ie *) frm;

		tie->tim_ie = IEEE80211_ELEMID_TIM;
		tie->tim_len = 4;	/* length */
		tie->tim_count = 0;	/* DTIM count */ 
		tie->tim_period = ic->ic_dtim_period;	/* DTIM period */
		tie->tim_bitctl = 0;	/* bitmap control */
		tie->tim_bitmap[0] = 0;	/* Partial Virtual Bitmap */
		frm += sizeof(struct ieee80211_tim_ie);
		bo->bo_tim_len = 1;
	}
	bo->bo_tim_trailer = frm;
	if (ic->ic_flags & IEEE80211_F_DOTH)
		frm = ieee80211_add_countryie(frm, ic,
			ic->ic_countrycode, ic->ic_location);
	if (ic->ic_flags & IEEE80211_F_WPA)
		frm = ieee80211_add_wpa(frm, ic);
	if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan)) {
		bo->bo_erp = frm;
		frm = ieee80211_add_erp(frm, ic);
	}
	frm = ieee80211_add_xrates(frm, rs);
	if (IEEE80211_IS_CHAN_HT(ic->ic_bsschan)) {
		frm = ieee80211_add_htcap(frm, ni);
		bo->bo_htinfo = frm;
		frm = ieee80211_add_htinfo(frm, ni);
	}
	if (ic->ic_flags & IEEE80211_F_WME) {
		bo->bo_wme = frm;
		frm = ieee80211_add_wme_param(frm, &ic->ic_wme);
	}
	if (IEEE80211_IS_CHAN_HT(ic->ic_bsschan) &&
	    (ic->ic_flags_ext & IEEE80211_FEXT_HTCOMPAT)) {
		frm = ieee80211_add_htcap_vendor(frm, ni);
		frm = ieee80211_add_htinfo_vendor(frm, ni);
	}
	bo->bo_tim_trailer_len = frm - bo->bo_tim_trailer;
	m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);

	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	KASSERT(m != NULL, ("no space for 802.11 header?"));
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(uint16_t *)wh->i_dur = 0;
	IEEE80211_ADDR_COPY(wh->i_addr1, ifp->if_broadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	*(uint16_t *)wh->i_seq = 0;

	return m;
}

/*
 * Update the dynamic parts of a beacon frame based on the current state.
 */
int
ieee80211_beacon_update(struct ieee80211_node *ni,
	struct ieee80211_beacon_offsets *bo, struct mbuf *m, int mcast)
{
	struct ieee80211com *ic = ni->ni_ic;
	int len_changed = 0;
	uint16_t capinfo;

	IEEE80211_BEACON_LOCK(ic);
	/* XXX faster to recalculate entirely or just changes? */
	capinfo = getcapinfo(ic, ni->ni_chan);
	*bo->bo_caps = htole16(capinfo);

	if (ic->ic_flags & IEEE80211_F_WME) {
		struct ieee80211_wme_state *wme = &ic->ic_wme;

		/*
		 * Check for agressive mode change.  When there is
		 * significant high priority traffic in the BSS
		 * throttle back BE traffic by using conservative
		 * parameters.  Otherwise BE uses agressive params
		 * to optimize performance of legacy/non-QoS traffic.
		 */
		if (wme->wme_flags & WME_F_AGGRMODE) {
			if (wme->wme_hipri_traffic >
			    wme->wme_hipri_switch_thresh) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
				    "%s: traffic %u, disable aggressive mode\n",
				    __func__, wme->wme_hipri_traffic);
				wme->wme_flags &= ~WME_F_AGGRMODE;
				ieee80211_wme_updateparams_locked(ic);
				wme->wme_hipri_traffic =
					wme->wme_hipri_switch_hysteresis;
			} else
				wme->wme_hipri_traffic = 0;
		} else {
			if (wme->wme_hipri_traffic <=
			    wme->wme_hipri_switch_thresh) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
				    "%s: traffic %u, enable aggressive mode\n",
				    __func__, wme->wme_hipri_traffic);
				wme->wme_flags |= WME_F_AGGRMODE;
				ieee80211_wme_updateparams_locked(ic);
				wme->wme_hipri_traffic = 0;
			} else
				wme->wme_hipri_traffic =
					wme->wme_hipri_switch_hysteresis;
		}
		if (isset(bo->bo_flags, IEEE80211_BEACON_WME)) {
			(void) ieee80211_add_wme_param(bo->bo_wme, wme);
			clrbit(bo->bo_flags, IEEE80211_BEACON_WME);
		}
	}

	if (isset(bo->bo_flags, IEEE80211_BEACON_HTINFO)) {
		ieee80211_ht_update_beacon(ic, bo);
		clrbit(bo->bo_flags, IEEE80211_BEACON_HTINFO);
	}

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {	/* NB: no IBSS support*/
		struct ieee80211_tim_ie *tie =
			(struct ieee80211_tim_ie *) bo->bo_tim;
		if (isset(bo->bo_flags, IEEE80211_BEACON_TIM)) {
			u_int timlen, timoff, i;
			/* 
			 * ATIM/DTIM needs updating.  If it fits in the
			 * current space allocated then just copy in the
			 * new bits.  Otherwise we need to move any trailing
			 * data to make room.  Note that we know there is
			 * contiguous space because ieee80211_beacon_allocate
			 * insures there is space in the mbuf to write a
			 * maximal-size virtual bitmap (based on ic_max_aid).
			 */
			/*
			 * Calculate the bitmap size and offset, copy any
			 * trailer out of the way, and then copy in the
			 * new bitmap and update the information element.
			 * Note that the tim bitmap must contain at least
			 * one byte and any offset must be even.
			 */
			if (ic->ic_ps_pending != 0) {
				timoff = 128;		/* impossibly large */
				for (i = 0; i < ic->ic_tim_len; i++)
					if (ic->ic_tim_bitmap[i]) {
						timoff = i &~ 1;
						break;
					}
				KASSERT(timoff != 128, ("tim bitmap empty!"));
				for (i = ic->ic_tim_len-1; i >= timoff; i--)
					if (ic->ic_tim_bitmap[i])
						break;
				timlen = 1 + (i - timoff);
			} else {
				timoff = 0;
				timlen = 1;
			}
			if (timlen != bo->bo_tim_len) {
				/* copy up/down trailer */
				int adjust = tie->tim_bitmap+timlen
					   - bo->bo_tim_trailer;
				ovbcopy(bo->bo_tim_trailer,
				    bo->bo_tim_trailer+adjust,
				    bo->bo_tim_trailer_len);
				bo->bo_tim_trailer += adjust;
				bo->bo_wme += adjust;
				bo->bo_erp += adjust;
				bo->bo_htinfo += adjust;
				bo->bo_tim_len = timlen;

				/* update information element */
				tie->tim_len = 3 + timlen;
				tie->tim_bitctl = timoff;
				len_changed = 1;
			}
			memcpy(tie->tim_bitmap, ic->ic_tim_bitmap + timoff,
				bo->bo_tim_len);

			clrbit(bo->bo_flags, IEEE80211_BEACON_TIM);

			IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
				"%s: TIM updated, pending %u, off %u, len %u\n",
				__func__, ic->ic_ps_pending, timoff, timlen);
		}
		/* count down DTIM period */
		if (tie->tim_count == 0)
			tie->tim_count = tie->tim_period - 1;
		else
			tie->tim_count--;
		/* update state for buffered multicast frames on DTIM */
		if (mcast && tie->tim_count == 0)
			tie->tim_bitctl |= 1;
		else
			tie->tim_bitctl &= ~1;
		if (isset(bo->bo_flags, IEEE80211_BEACON_ERP)) {
			/*
			 * ERP element needs updating.
			 */
			(void) ieee80211_add_erp(bo->bo_erp, ic);
			clrbit(bo->bo_flags, IEEE80211_BEACON_ERP);
		}
	}
	IEEE80211_BEACON_UNLOCK(ic);

	return len_changed;
}
