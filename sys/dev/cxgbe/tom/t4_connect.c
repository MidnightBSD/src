/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/toecore.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

/* atid services */
static int alloc_atid(struct adapter *, void *);
static void *lookup_atid(struct adapter *, int);
static void free_atid(struct adapter *, int);

static int
alloc_atid(struct adapter *sc, void *ctx)
{
	struct tid_info *t = &sc->tids;
	int atid = -1;

	mtx_lock(&t->atid_lock);
	if (t->afree) {
		union aopen_entry *p = t->afree;

		atid = p - t->atid_tab;
		t->afree = p->next;
		p->data = ctx;
		t->atids_in_use++;
	}
	mtx_unlock(&t->atid_lock);
	return (atid);
}

static void *
lookup_atid(struct adapter *sc, int atid)
{
	struct tid_info *t = &sc->tids;

	return (t->atid_tab[atid].data);
}

static void
free_atid(struct adapter *sc, int atid)
{
	struct tid_info *t = &sc->tids;
	union aopen_entry *p = &t->atid_tab[atid];

	mtx_lock(&t->atid_lock);
	p->next = t->afree;
	t->afree = p;
	t->atids_in_use--;
	mtx_unlock(&t->atid_lock);
}

/*
 * Active open failed.
 */
static int
do_act_establish(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_act_establish *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	unsigned int atid = G_TID_TID(ntohl(cpl->tos_atid));
	struct toepcb *toep = lookup_atid(sc, atid);
	struct inpcb *inp = toep->inp;

	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == atid, ("%s: toep tid/atid mismatch", __func__));

	CTR3(KTR_CXGBE, "%s: atid %u, tid %u", __func__, atid, tid);
	free_atid(sc, atid);

	INP_WLOCK(inp);
	toep->tid = tid;
	insert_tid(sc, tid, toep);
	if (inp->inp_flags & INP_DROPPED) {

		/* socket closed by the kernel before hw told us it connected */

		send_flowc_wr(toep, NULL);
		send_reset(sc, toep, be32toh(cpl->snd_isn));
		goto done;
	}

	make_established(toep, cpl->snd_isn, cpl->rcv_isn, cpl->tcp_opt);
done:
	INP_WUNLOCK(inp);
	return (0);
}

static inline int
act_open_has_tid(unsigned int status)
{

	return (status != CPL_ERR_TCAM_FULL &&
	    status != CPL_ERR_TCAM_PARITY &&
	    status != CPL_ERR_CONN_EXIST &&
	    status != CPL_ERR_ARP_MISS);
}

/*
 * Convert an ACT_OPEN_RPL status to an errno.
 */
static inline int
act_open_rpl_status_to_errno(int status)
{

	switch (status) {
	case CPL_ERR_CONN_RESET:
		return (ECONNREFUSED);
	case CPL_ERR_ARP_MISS:
		return (EHOSTUNREACH);
	case CPL_ERR_CONN_TIMEDOUT:
		return (ETIMEDOUT);
	case CPL_ERR_TCAM_FULL:
		return (ENOMEM);
	case CPL_ERR_CONN_EXIST:
		log(LOG_ERR, "ACTIVE_OPEN_RPL: 4-tuple in use\n");
		return (EADDRINUSE);
	default:
		return (EIO);
	}
}

static int
do_act_open_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_act_open_rpl *cpl = (const void *)(rss + 1);
	unsigned int atid = G_TID_TID(G_AOPEN_ATID(be32toh(cpl->atid_status)));
	unsigned int status = G_AOPEN_STATUS(be32toh(cpl->atid_status));
	struct toepcb *toep = lookup_atid(sc, atid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);
	struct toedev *tod = &toep->td->tod;

	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == atid, ("%s: toep tid/atid mismatch", __func__));

	CTR3(KTR_CXGBE, "%s: atid %u, status %u ", __func__, atid, status);

	/* Ignore negative advice */
	if (status == CPL_ERR_RTX_NEG_ADVICE)
		return (0);

	free_atid(sc, atid);
	toep->tid = -1;

	if (status && act_open_has_tid(status))
		release_tid(sc, GET_TID(cpl), toep->ctrlq);

	if (status == CPL_ERR_TCAM_FULL) {
		INP_WLOCK(inp);
		toe_connect_failed(tod, tp, EAGAIN);
		final_cpl_received(toep);	/* unlocks inp */
	} else {
		INP_INFO_WLOCK(&V_tcbinfo);
		INP_WLOCK(inp);
		toe_connect_failed(tod, tp, act_open_rpl_status_to_errno(status));
		final_cpl_received(toep);	/* unlocks inp */
		INP_INFO_WUNLOCK(&V_tcbinfo);
	}

	return (0);
}

/*
 * Options2 for active open.
 */
static uint32_t
calc_opt2a(struct socket *so)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct port_info *pi = toep->port;
	struct adapter *sc = pi->adapter;
	uint32_t opt2 = 0;

	if (tp->t_flags & TF_SACK_PERMIT)
		opt2 |= F_SACK_EN;

	if (tp->t_flags & TF_REQ_TSTMP)
		opt2 |= F_TSTAMPS_EN;

	if (tp->t_flags & TF_REQ_SCALE)
		opt2 |= F_WND_SCALE_EN;

	if (V_tcp_do_ecn)
		opt2 |= F_CCTRL_ECN;

	opt2 |= V_TX_QUEUE(sc->params.tp.tx_modq[pi->tx_chan]);
	opt2 |= F_RX_COALESCE_VALID | V_RX_COALESCE(M_RX_COALESCE);
	opt2 |= F_RSS_QUEUE_VALID | V_RSS_QUEUE(toep->ofld_rxq->iq.abs_id);

	return (htobe32(opt2));
}


void
t4_init_connect_cpl_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_ACT_ESTABLISH, do_act_establish);
	t4_register_cpl_handler(sc, CPL_ACT_OPEN_RPL, do_act_open_rpl);
}

/*
 * active open (soconnect).
 *
 * State of affairs on entry:
 * soisconnecting (so_state |= SS_ISCONNECTING)
 * tcbinfo not locked (This has changed - used to be WLOCKed)
 * inp WLOCKed
 * tp->t_state = TCPS_SYN_SENT
 * rtalloc1, RT_UNLOCK on rt.
 */
int
t4_connect(struct toedev *tod, struct socket *so, struct rtentry *rt,
    struct sockaddr *nam)
{
	struct adapter *sc = tod->tod_softc;
	struct toepcb *toep = NULL;
	struct wrqe *wr = NULL;
	struct cpl_act_open_req *cpl;
	struct l2t_entry *e = NULL;
	struct ifnet *rt_ifp = rt->rt_ifp;
	struct port_info *pi;
	int atid = -1, mtu_idx, rscale, qid_atid, rc = ENOMEM;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);

	INP_WLOCK_ASSERT(inp);

	if (nam->sa_family != AF_INET)
		CXGBE_UNIMPLEMENTED("IPv6 connect");

	if (rt_ifp->if_type == IFT_ETHER)
		pi = rt_ifp->if_softc;
	else if (rt_ifp->if_type == IFT_L2VLAN) {
		struct ifnet *ifp = VLAN_COOKIE(rt_ifp);

		pi = ifp->if_softc;
	} else if (rt_ifp->if_type == IFT_IEEE8023ADLAG)
		return (ENOSYS);	/* XXX: implement lagg support */
	else
		return (ENOTSUP);

	toep = alloc_toepcb(pi, -1, -1, M_NOWAIT);
	if (toep == NULL)
		goto failed;

	atid = alloc_atid(sc, toep);
	if (atid < 0)
		goto failed;

	e = t4_l2t_get(pi, rt_ifp,
	    rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway : nam);
	if (e == NULL)
		goto failed;

	wr = alloc_wrqe(sizeof(*cpl), toep->ctrlq);
	if (wr == NULL)
		goto failed;
	cpl = wrtod(wr);

	toep->tid = atid;
	toep->l2te = e;
	toep->ulp_mode = ULP_MODE_NONE;
	SOCKBUF_LOCK(&so->so_rcv);
	/* opt0 rcv_bufsiz initially, assumes its normal meaning later */
	toep->rx_credits = min(select_rcv_wnd(so) >> 10, M_RCV_BUFSIZ);
	SOCKBUF_UNLOCK(&so->so_rcv);

	offload_socket(so, toep);

	/*
	 * The kernel sets request_r_scale based on sb_max whereas we need to
	 * take hardware's MAX_RCV_WND into account too.  This is normally a
	 * no-op as MAX_RCV_WND is much larger than the default sb_max.
	 */
	if (tp->t_flags & TF_REQ_SCALE)
		rscale = tp->request_r_scale = select_rcv_wscale();
	else
		rscale = 0;
	mtu_idx = find_best_mtu_idx(sc, &inp->inp_inc, 0);
	qid_atid = (toep->ofld_rxq->iq.abs_id << 14) | atid;

	INIT_TP_WR(cpl, 0);
	OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ, qid_atid));
	inp_4tuple_get(inp, &cpl->local_ip, &cpl->local_port, &cpl->peer_ip,
	    &cpl->peer_port);
	cpl->opt0 = calc_opt0(so, pi, e, mtu_idx, rscale, toep->rx_credits,
	    toep->ulp_mode);
	cpl->params = select_ntuple(pi, e, sc->filter_mode);
	cpl->opt2 = calc_opt2a(so);

	CTR5(KTR_CXGBE, "%s: atid %u (%s), toep %p, inp %p", __func__,
	    toep->tid, tcpstates[tp->t_state], toep, inp);

	rc = t4_l2t_send(sc, wr, e);
	if (rc == 0) {
		toepcb_set_flag(toep, TPF_CPL_PENDING);
		return (0);
	}

	undo_offload_socket(so);
failed:
	CTR5(KTR_CXGBE, "%s: FAILED, atid %d, toep %p, l2te %p, wr %p",
	    __func__, atid, toep, e, wr);

	if (e)
		t4_l2t_release(e);
	if (wr)
		free_wrqe(wr);
	if (atid >= 0)
		free_atid(sc, atid);
	if (toep)
		free_toepcb(toep);

	return (rc);
}
#endif
