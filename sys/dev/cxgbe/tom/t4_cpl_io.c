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
__FBSDID("$FreeBSD: stable/9/sys/dev/cxgbe/tom/t4_cpl_io.c 247434 2013-02-28 00:44:54Z np $");

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
#include <sys/sglist.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/toecore.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_tcb.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

VNET_DECLARE(int, tcp_do_autosndbuf);
#define V_tcp_do_autosndbuf VNET(tcp_do_autosndbuf)
VNET_DECLARE(int, tcp_autosndbuf_inc);
#define V_tcp_autosndbuf_inc VNET(tcp_autosndbuf_inc)
VNET_DECLARE(int, tcp_autosndbuf_max);
#define V_tcp_autosndbuf_max VNET(tcp_autosndbuf_max)
VNET_DECLARE(int, tcp_do_autorcvbuf);
#define V_tcp_do_autorcvbuf VNET(tcp_do_autorcvbuf)
VNET_DECLARE(int, tcp_autorcvbuf_inc);
#define V_tcp_autorcvbuf_inc VNET(tcp_autorcvbuf_inc)
VNET_DECLARE(int, tcp_autorcvbuf_max);
#define V_tcp_autorcvbuf_max VNET(tcp_autorcvbuf_max)

void
send_flowc_wr(struct toepcb *toep, struct flowc_tx_params *ftxp)
{
	struct wrqe *wr;
	struct fw_flowc_wr *flowc;
	unsigned int nparams = ftxp ? 8 : 6, flowclen;
	struct port_info *pi = toep->port;
	struct adapter *sc = pi->adapter;
	unsigned int pfvf = G_FW_VIID_PFN(pi->viid) << S_FW_VIID_PFN;
	struct ofld_tx_sdesc *txsd = &toep->txsd[toep->txsd_pidx];

	KASSERT(!(toep->flags & TPF_FLOWC_WR_SENT),
	    ("%s: flowc for tid %u sent already", __func__, toep->tid));

	CTR2(KTR_CXGBE, "%s: tid %u", __func__, toep->tid);

	flowclen = sizeof(*flowc) + nparams * sizeof(struct fw_flowc_mnemval);

	wr = alloc_wrqe(roundup(flowclen, 16), toep->ofld_txq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	flowc = wrtod(wr);
	memset(flowc, 0, wr->wr_len);

	flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
	    V_FW_FLOWC_WR_NPARAMS(nparams));
	flowc->flowid_len16 = htonl(V_FW_WR_LEN16(howmany(flowclen, 16)) |
	    V_FW_WR_FLOWID(toep->tid));

	flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_PFNVFN;
	flowc->mnemval[0].val = htobe32(pfvf);
	flowc->mnemval[1].mnemonic = FW_FLOWC_MNEM_CH;
	flowc->mnemval[1].val = htobe32(pi->tx_chan);
	flowc->mnemval[2].mnemonic = FW_FLOWC_MNEM_PORT;
	flowc->mnemval[2].val = htobe32(pi->tx_chan);
	flowc->mnemval[3].mnemonic = FW_FLOWC_MNEM_IQID;
	flowc->mnemval[3].val = htobe32(toep->ofld_rxq->iq.abs_id);
	if (ftxp) {
		uint32_t sndbuf = min(ftxp->snd_space, sc->tt.sndbuf);

		flowc->mnemval[4].mnemonic = FW_FLOWC_MNEM_SNDNXT;
		flowc->mnemval[4].val = htobe32(ftxp->snd_nxt);
		flowc->mnemval[5].mnemonic = FW_FLOWC_MNEM_RCVNXT;
		flowc->mnemval[5].val = htobe32(ftxp->rcv_nxt);
		flowc->mnemval[6].mnemonic = FW_FLOWC_MNEM_SNDBUF;
		flowc->mnemval[6].val = htobe32(sndbuf);
		flowc->mnemval[7].mnemonic = FW_FLOWC_MNEM_MSS;
		flowc->mnemval[7].val = htobe32(ftxp->mss);
	} else {
		flowc->mnemval[4].mnemonic = FW_FLOWC_MNEM_SNDBUF;
		flowc->mnemval[4].val = htobe32(512);
		flowc->mnemval[5].mnemonic = FW_FLOWC_MNEM_MSS;
		flowc->mnemval[5].val = htobe32(512);
	}

	txsd->tx_credits = howmany(flowclen, 16);
	txsd->plen = 0;
	KASSERT(toep->tx_credits >= txsd->tx_credits && toep->txsd_avail > 0,
	    ("%s: not enough credits (%d)", __func__, toep->tx_credits));
	toep->tx_credits -= txsd->tx_credits;
	if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
		toep->txsd_pidx = 0;
	toep->txsd_avail--;

	toep->flags |= TPF_FLOWC_WR_SENT;
        t4_wrq_tx(sc, wr);
}

void
send_reset(struct adapter *sc, struct toepcb *toep, uint32_t snd_nxt)
{
	struct wrqe *wr;
	struct cpl_abort_req *req;
	int tid = toep->tid;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);	/* don't use if INP_DROPPED */

	INP_WLOCK_ASSERT(inp);

	CTR6(KTR_CXGBE, "%s: tid %d (%s), toep_flags 0x%x, inp_flags 0x%x%s",
	    __func__, toep->tid,
	    inp->inp_flags & INP_DROPPED ? "inp dropped" :
	    tcpstates[tp->t_state],
	    toep->flags, inp->inp_flags,
	    toep->flags & TPF_ABORT_SHUTDOWN ?
	    " (abort already in progress)" : "");

	if (toep->flags & TPF_ABORT_SHUTDOWN)
		return;	/* abort already in progress */

	toep->flags |= TPF_ABORT_SHUTDOWN;

	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %d.", __func__, tid));

	wr = alloc_wrqe(sizeof(*req), toep->ofld_txq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_ABORT_REQ, tid);
	if (inp->inp_flags & INP_DROPPED)
		req->rsvd0 = htobe32(snd_nxt);
	else
		req->rsvd0 = htobe32(tp->snd_nxt);
	req->rsvd1 = !(toep->flags & TPF_TX_DATA_SENT);
	req->cmd = CPL_ABORT_SEND_RST;

	/*
	 * XXX: What's the correct way to tell that the inp hasn't been detached
	 * from its socket?  Should I even be flushing the snd buffer here?
	 */
	if ((inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) == 0) {
		struct socket *so = inp->inp_socket;

		if (so != NULL)	/* because I'm not sure.  See comment above */
			sbflush(&so->so_snd);
	}

	t4_l2t_send(sc, wr, toep->l2te);
}

/*
 * Called when a connection is established to translate the TCP options
 * reported by HW to FreeBSD's native format.
 */
static void
assign_rxopt(struct tcpcb *tp, unsigned int opt)
{
	struct toepcb *toep = tp->t_toe;
	struct adapter *sc = td_adapter(toep->td);

	INP_LOCK_ASSERT(tp->t_inpcb);

	tp->t_maxseg = tp->t_maxopd = sc->params.mtus[G_TCPOPT_MSS(opt)] - 40;

	if (G_TCPOPT_TSTAMP(opt)) {
		tp->t_flags |= TF_RCVD_TSTMP;	/* timestamps ok */
		tp->ts_recent = 0;		/* hmmm */
		tp->ts_recent_age = tcp_ts_getticks();
		tp->t_maxseg -= TCPOLEN_TSTAMP_APPA;
	}

	if (G_TCPOPT_SACK(opt))
		tp->t_flags |= TF_SACK_PERMIT;	/* should already be set */
	else
		tp->t_flags &= ~TF_SACK_PERMIT;	/* sack disallowed by peer */

	if (G_TCPOPT_WSCALE_OK(opt))
		tp->t_flags |= TF_RCVD_SCALE;

	/* Doing window scaling? */
	if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE | TF_REQ_SCALE)) {
		tp->rcv_scale = tp->request_r_scale;
		tp->snd_scale = G_TCPOPT_SND_WSCALE(opt);
	}
}

/*
 * Completes some final bits of initialization for just established connections
 * and changes their state to TCPS_ESTABLISHED.
 *
 * The ISNs are from after the exchange of SYNs.  i.e., the true ISN + 1.
 */
void
make_established(struct toepcb *toep, uint32_t snd_isn, uint32_t rcv_isn,
    uint16_t opt)
{
	struct inpcb *inp = toep->inp;
	struct socket *so = inp->inp_socket;
	struct tcpcb *tp = intotcpcb(inp);
	long bufsize;
	uint32_t iss = be32toh(snd_isn) - 1;	/* true ISS */
	uint32_t irs = be32toh(rcv_isn) - 1;	/* true IRS */
	uint16_t tcpopt = be16toh(opt);
	struct flowc_tx_params ftxp;

	INP_WLOCK_ASSERT(inp);
	KASSERT(tp->t_state == TCPS_SYN_SENT ||
	    tp->t_state == TCPS_SYN_RECEIVED,
	    ("%s: TCP state %s", __func__, tcpstates[tp->t_state]));

	CTR4(KTR_CXGBE, "%s: tid %d, toep %p, inp %p",
	    __func__, toep->tid, toep, inp);

	tp->t_state = TCPS_ESTABLISHED;
	tp->t_starttime = ticks;
	TCPSTAT_INC(tcps_connects);

	tp->irs = irs;
	tcp_rcvseqinit(tp);
	tp->rcv_wnd = toep->rx_credits << 10;
	tp->rcv_adv += tp->rcv_wnd;
	tp->last_ack_sent = tp->rcv_nxt;

	/*
	 * If we were unable to send all rx credits via opt0, save the remainder
	 * in rx_credits so that they can be handed over with the next credit
	 * update.
	 */
	SOCKBUF_LOCK(&so->so_rcv);
	bufsize = select_rcv_wnd(so);
	SOCKBUF_UNLOCK(&so->so_rcv);
	toep->rx_credits = bufsize - tp->rcv_wnd;

	tp->iss = iss;
	tcp_sendseqinit(tp);
	tp->snd_una = iss + 1;
	tp->snd_nxt = iss + 1;
	tp->snd_max = iss + 1;

	assign_rxopt(tp, tcpopt);

	SOCKBUF_LOCK(&so->so_snd);
	if (so->so_snd.sb_flags & SB_AUTOSIZE && V_tcp_do_autosndbuf)
		bufsize = V_tcp_autosndbuf_max;
	else
		bufsize = sbspace(&so->so_snd);
	SOCKBUF_UNLOCK(&so->so_snd);

	ftxp.snd_nxt = tp->snd_nxt;
	ftxp.rcv_nxt = tp->rcv_nxt;
	ftxp.snd_space = bufsize;
	ftxp.mss = tp->t_maxseg;
	send_flowc_wr(toep, &ftxp);

	soisconnected(so);
}

static int
send_rx_credits(struct adapter *sc, struct toepcb *toep, int credits)
{
	struct wrqe *wr;
	struct cpl_rx_data_ack *req;
	uint32_t dack = F_RX_DACK_CHANGE | V_RX_DACK_MODE(1);

	KASSERT(credits >= 0, ("%s: %d credits", __func__, credits));

	wr = alloc_wrqe(sizeof(*req), toep->ctrlq);
	if (wr == NULL)
		return (0);
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_RX_DATA_ACK, toep->tid);
	req->credit_dack = htobe32(dack | V_RX_CREDITS(credits));

	t4_wrq_tx(sc, wr);
	return (credits);
}

void
t4_rcvd(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	struct sockbuf *sb = &so->so_rcv;
	struct toepcb *toep = tp->t_toe;
	int credits;

	INP_WLOCK_ASSERT(inp);

	SOCKBUF_LOCK(sb);
	KASSERT(toep->sb_cc >= sb->sb_cc,
	    ("%s: sb %p has more data (%d) than last time (%d).",
	    __func__, sb, sb->sb_cc, toep->sb_cc));
	toep->rx_credits += toep->sb_cc - sb->sb_cc;
	toep->sb_cc = sb->sb_cc;
	credits = toep->rx_credits;
	SOCKBUF_UNLOCK(sb);

	if (credits > 0 &&
	    (credits + 16384 >= tp->rcv_wnd || credits >= 15 * 1024)) {

		credits = send_rx_credits(sc, toep, credits);
		SOCKBUF_LOCK(sb);
		toep->rx_credits -= credits;
		SOCKBUF_UNLOCK(sb);
		tp->rcv_wnd += credits;
		tp->rcv_adv += credits;
	}
}

/*
 * Close a connection by sending a CPL_CLOSE_CON_REQ message.
 */
static int
close_conn(struct adapter *sc, struct toepcb *toep)
{
	struct wrqe *wr;
	struct cpl_close_con_req *req;
	unsigned int tid = toep->tid;

	CTR3(KTR_CXGBE, "%s: tid %u%s", __func__, toep->tid,
	    toep->flags & TPF_FIN_SENT ? ", IGNORED" : "");

	if (toep->flags & TPF_FIN_SENT)
		return (0);

	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %u.", __func__, tid));

	wr = alloc_wrqe(sizeof(*req), toep->ofld_txq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

        req->wr.wr_hi = htonl(V_FW_WR_OP(FW_TP_WR) |
	    V_FW_WR_IMMDLEN(sizeof(*req) - sizeof(req->wr)));
	req->wr.wr_mid = htonl(V_FW_WR_LEN16(howmany(sizeof(*req), 16)) |
	    V_FW_WR_FLOWID(tid));
        req->wr.wr_lo = cpu_to_be64(0);
        OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_CON_REQ, tid));
	req->rsvd = 0;

	toep->flags |= TPF_FIN_SENT;
	toep->flags &= ~TPF_SEND_FIN;
	t4_l2t_send(sc, wr, toep->l2te);

	return (0);
}

#define MAX_OFLD_TX_CREDITS (SGE_MAX_WR_LEN / 16)
#define MIN_OFLD_TX_CREDITS (howmany(sizeof(struct fw_ofld_tx_data_wr) + 1, 16))

/* Maximum amount of immediate data we could stuff in a WR */
static inline int
max_imm_payload(int tx_credits)
{
	const int n = 2;	/* Use only up to 2 desc for imm. data WR */

	KASSERT(tx_credits >= 0 &&
		tx_credits <= MAX_OFLD_TX_CREDITS,
		("%s: %d credits", __func__, tx_credits));

	if (tx_credits < MIN_OFLD_TX_CREDITS)
		return (0);

	if (tx_credits >= (n * EQ_ESIZE) / 16)
		return ((n * EQ_ESIZE) - sizeof(struct fw_ofld_tx_data_wr));
	else
		return (tx_credits * 16 - sizeof(struct fw_ofld_tx_data_wr));
}

/* Maximum number of SGL entries we could stuff in a WR */
static inline int
max_dsgl_nsegs(int tx_credits)
{
	int nseg = 1;	/* ulptx_sgl has room for 1, rest ulp_tx_sge_pair */
	int sge_pair_credits = tx_credits - MIN_OFLD_TX_CREDITS;

	KASSERT(tx_credits >= 0 &&
		tx_credits <= MAX_OFLD_TX_CREDITS,
		("%s: %d credits", __func__, tx_credits));

	if (tx_credits < MIN_OFLD_TX_CREDITS)
		return (0);

	nseg += 2 * (sge_pair_credits * 16 / 24);
	if ((sge_pair_credits * 16) % 24 == 16)
		nseg++;

	return (nseg);
}

static inline void
write_tx_wr(void *dst, struct toepcb *toep, unsigned int immdlen,
    unsigned int plen, uint8_t credits, int more_to_come)
{
	struct fw_ofld_tx_data_wr *txwr = dst;
	int shove = !more_to_come;
	int compl = 1;

	/*
	 * We always request completion notifications from the firmware.  The
	 * only exception is when we know we'll get more data to send shortly
	 * and that we'll have some tx credits remaining to transmit that data.
	 */
	if (more_to_come && toep->tx_credits - credits >= MIN_OFLD_TX_CREDITS)
		compl = 0;

	txwr->op_to_immdlen = htobe32(V_WR_OP(FW_OFLD_TX_DATA_WR) |
	    V_FW_WR_COMPL(compl) | V_FW_WR_IMMDLEN(immdlen));
	txwr->flowid_len16 = htobe32(V_FW_WR_FLOWID(toep->tid) |
	    V_FW_WR_LEN16(credits));
	txwr->tunnel_to_proxy =
	    htobe32(V_FW_OFLD_TX_DATA_WR_ULPMODE(toep->ulp_mode) |
		V_FW_OFLD_TX_DATA_WR_URGENT(0) |	/* XXX */
		V_FW_OFLD_TX_DATA_WR_SHOVE(shove));
	txwr->plen = htobe32(plen);
}

/*
 * Generate a DSGL from a starting mbuf.  The total number of segments and the
 * maximum segments in any one mbuf are provided.
 */
static void
write_tx_sgl(void *dst, struct mbuf *start, struct mbuf *stop, int nsegs, int n)
{
	struct mbuf *m;
	struct ulptx_sgl *usgl = dst;
	int i, j, rc;
	struct sglist sg;
	struct sglist_seg segs[n];

	KASSERT(nsegs > 0, ("%s: nsegs 0", __func__));

	sglist_init(&sg, n, segs);
	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(nsegs));

	i = -1;
	for (m = start; m != stop; m = m->m_next) {
		rc = sglist_append(&sg, mtod(m, void *), m->m_len);
		if (__predict_false(rc != 0))
			panic("%s: sglist_append %d", __func__, rc);

		for (j = 0; j < sg.sg_nseg; i++, j++) {
			if (i < 0) {
				usgl->len0 = htobe32(segs[j].ss_len);
				usgl->addr0 = htobe64(segs[j].ss_paddr);
			} else {
				usgl->sge[i / 2].len[i & 1] =
				    htobe32(segs[j].ss_len);
				usgl->sge[i / 2].addr[i & 1] =
				    htobe64(segs[j].ss_paddr);
			}
#ifdef INVARIANTS
			nsegs--;
#endif
		}
		sglist_reset(&sg);
	}
	if (i & 1)
		usgl->sge[i / 2].len[1] = htobe32(0);
	KASSERT(nsegs == 0, ("%s: nsegs %d, start %p, stop %p",
	    __func__, nsegs, start, stop));
}

/*
 * Max number of SGL entries an offload tx work request can have.  This is 41
 * (1 + 40) for a full 512B work request.
 * fw_ofld_tx_data_wr(16B) + ulptx_sgl(16B, 1) + ulptx_sge_pair(480B, 40)
 */
#define OFLD_SGL_LEN (41)

/*
 * Send data and/or a FIN to the peer.
 *
 * The socket's so_snd buffer consists of a stream of data starting with sb_mb
 * and linked together with m_next.  sb_sndptr, if set, is the last mbuf that
 * was transmitted.
 */
static void
t4_push_frames(struct adapter *sc, struct toepcb *toep)
{
	struct mbuf *sndptr, *m, *sb_sndptr;
	struct fw_ofld_tx_data_wr *txwr;
	struct wrqe *wr;
	unsigned int plen, nsegs, credits, max_imm, max_nsegs, max_nsegs_1mbuf;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);
	struct socket *so = inp->inp_socket;
	struct sockbuf *sb = &so->so_snd;
	int tx_credits;
	struct ofld_tx_sdesc *txsd = &toep->txsd[toep->txsd_pidx];

	INP_WLOCK_ASSERT(inp);
	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %u.", __func__, toep->tid));

	if (__predict_false(toep->ulp_mode != ULP_MODE_NONE &&
	    toep->ulp_mode != ULP_MODE_TCPDDP))
		CXGBE_UNIMPLEMENTED("ulp_mode");

	/*
	 * This function doesn't resume by itself.  Someone else must clear the
	 * flag and call this function.
	 */
	if (__predict_false(toep->flags & TPF_TX_SUSPENDED))
		return;

	do {
		tx_credits = min(toep->tx_credits, MAX_OFLD_TX_CREDITS);
		max_imm = max_imm_payload(tx_credits);
		max_nsegs = max_dsgl_nsegs(tx_credits);

		SOCKBUF_LOCK(sb);
		sb_sndptr = sb->sb_sndptr;
		sndptr = sb_sndptr ? sb_sndptr->m_next : sb->sb_mb;
		plen = 0;
		nsegs = 0;
		max_nsegs_1mbuf = 0; /* max # of SGL segments in any one mbuf */
		for (m = sndptr; m != NULL; m = m->m_next) {
			int n = sglist_count(mtod(m, void *), m->m_len);

			nsegs += n;
			plen += m->m_len;

			/* This mbuf sent us _over_ the nsegs limit, back out */
			if (plen > max_imm && nsegs > max_nsegs) {
				nsegs -= n;
				plen -= m->m_len;
				if (plen == 0) {
					/* Too few credits */
					toep->flags |= TPF_TX_SUSPENDED;
					SOCKBUF_UNLOCK(sb);
					return;
				}
				break;
			}

			if (max_nsegs_1mbuf < n)
				max_nsegs_1mbuf = n;
			sb_sndptr = m;	/* new sb->sb_sndptr if all goes well */

			/* This mbuf put us right at the max_nsegs limit */
			if (plen > max_imm && nsegs == max_nsegs) {
				m = m->m_next;
				break;
			}
		}

		if (sb->sb_flags & SB_AUTOSIZE &&
		    V_tcp_do_autosndbuf &&
		    sb->sb_hiwat < V_tcp_autosndbuf_max &&
		    sbspace(sb) < sb->sb_hiwat / 8 * 7) {
			int newsize = min(sb->sb_hiwat + V_tcp_autosndbuf_inc,
			    V_tcp_autosndbuf_max);

			if (!sbreserve_locked(sb, newsize, so, NULL))
				sb->sb_flags &= ~SB_AUTOSIZE;
			else {
				sowwakeup_locked(so);	/* room available */
				SOCKBUF_UNLOCK_ASSERT(sb);
				goto unlocked;
			}
		}
		SOCKBUF_UNLOCK(sb);
unlocked:

		/* nothing to send */
		if (plen == 0) {
			KASSERT(m == NULL,
			    ("%s: nothing to send, but m != NULL", __func__));
			break;
		}

		if (__predict_false(toep->flags & TPF_FIN_SENT))
			panic("%s: excess tx.", __func__);

		if (plen <= max_imm) {

			/* Immediate data tx */

			wr = alloc_wrqe(roundup(sizeof(*txwr) + plen, 16),
					toep->ofld_txq);
			if (wr == NULL) {
				/* XXX: how will we recover from this? */
				toep->flags |= TPF_TX_SUSPENDED;
				return;
			}
			txwr = wrtod(wr);
			credits = howmany(wr->wr_len, 16);
			write_tx_wr(txwr, toep, plen, plen, credits,
			    tp->t_flags & TF_MORETOCOME);
			m_copydata(sndptr, 0, plen, (void *)(txwr + 1));
		} else {
			int wr_len;

			/* DSGL tx */

			wr_len = sizeof(*txwr) + sizeof(struct ulptx_sgl) +
			    ((3 * (nsegs - 1)) / 2 + ((nsegs - 1) & 1)) * 8;
			wr = alloc_wrqe(roundup(wr_len, 16), toep->ofld_txq);
			if (wr == NULL) {
				/* XXX: how will we recover from this? */
				toep->flags |= TPF_TX_SUSPENDED;
				return;
			}
			txwr = wrtod(wr);
			credits = howmany(wr_len, 16);
			write_tx_wr(txwr, toep, 0, plen, credits,
			    tp->t_flags & TF_MORETOCOME);
			write_tx_sgl(txwr + 1, sndptr, m, nsegs,
			    max_nsegs_1mbuf);
			if (wr_len & 0xf) {
				uint64_t *pad = (uint64_t *)
				    ((uintptr_t)txwr + wr_len);
				*pad = 0;
			}
		}

		KASSERT(toep->tx_credits >= credits,
			("%s: not enough credits", __func__));

		toep->tx_credits -= credits;

		tp->snd_nxt += plen;
		tp->snd_max += plen;

		SOCKBUF_LOCK(sb);
		KASSERT(sb_sndptr, ("%s: sb_sndptr is NULL", __func__));
		sb->sb_sndptr = sb_sndptr;
		SOCKBUF_UNLOCK(sb);

		toep->flags |= TPF_TX_DATA_SENT;

		KASSERT(toep->txsd_avail > 0, ("%s: no txsd", __func__));
		txsd->plen = plen;
		txsd->tx_credits = credits;
		txsd++;
		if (__predict_false(++toep->txsd_pidx == toep->txsd_total)) {
			toep->txsd_pidx = 0;
			txsd = &toep->txsd[0];
		}
		toep->txsd_avail--;

		t4_l2t_send(sc, wr, toep->l2te);
	} while (m != NULL);

	/* Send a FIN if requested, but only if there's no more data to send */
	if (m == NULL && toep->flags & TPF_SEND_FIN)
		close_conn(sc, toep);
}

int
t4_tod_output(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
#ifdef INVARIANTS
	struct inpcb *inp = tp->t_inpcb;
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp %p dropped.", __func__, inp));
	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));

	t4_push_frames(sc, toep);

	return (0);
}

int
t4_send_fin(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
#ifdef INVARIANTS
	struct inpcb *inp = tp->t_inpcb;
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp %p dropped.", __func__, inp));
	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));

	toep->flags |= TPF_SEND_FIN;
	t4_push_frames(sc, toep);

	return (0);
}

int
t4_send_rst(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
#if defined(INVARIANTS)
	struct inpcb *inp = tp->t_inpcb;
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp %p dropped.", __func__, inp));
	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));

	/* hmmmm */
	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc for tid %u [%s] not sent already",
	    __func__, toep->tid, tcpstates[tp->t_state]));

	send_reset(sc, toep, 0);
	return (0);
}

/*
 * Peer has sent us a FIN.
 */
static int
do_peer_close(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_peer_close *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = NULL;
	struct socket *so;
	struct sockbuf *sb;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PEER_CLOSE,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (__predict_false(toep->flags & TPF_SYNQE)) {
#ifdef INVARIANTS
		struct synq_entry *synqe = (void *)toep;

		INP_WLOCK(synqe->lctx->inp);
		if (synqe->flags & TPF_SYNQE_HAS_L2TE) {
			KASSERT(synqe->flags & TPF_ABORT_SHUTDOWN,
			    ("%s: listen socket closed but tid %u not aborted.",
			    __func__, tid));
		} else {
			/*
			 * do_pass_accept_req is still running and will
			 * eventually take care of this tid.
			 */
		}
		INP_WUNLOCK(synqe->lctx->inp);
#endif
		CTR4(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x)", __func__, tid,
		    toep, toep->flags);
		return (0);
	}

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	CTR5(KTR_CXGBE, "%s: tid %u (%s), toep_flags 0x%x, inp %p", __func__,
	    tid, tp ? tcpstates[tp->t_state] : "no tp", toep->flags, inp);

	if (toep->flags & TPF_ABORT_SHUTDOWN)
		goto done;

	tp->rcv_nxt++;	/* FIN */

	so = inp->inp_socket;
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	if (__predict_false(toep->ddp_flags & (DDP_BUF0_ACTIVE | DDP_BUF1_ACTIVE))) {
		m = m_get(M_NOWAIT, MT_DATA);
		if (m == NULL)
			CXGBE_UNIMPLEMENTED("mbuf alloc failure");

		m->m_len = be32toh(cpl->rcv_nxt) - tp->rcv_nxt;
		m->m_flags |= M_DDP;	/* Data is already where it should be */
		m->m_data = "nothing to see here";
		tp->rcv_nxt = be32toh(cpl->rcv_nxt);

		toep->ddp_flags &= ~(DDP_BUF0_ACTIVE | DDP_BUF1_ACTIVE);

		KASSERT(toep->sb_cc >= sb->sb_cc,
		    ("%s: sb %p has more data (%d) than last time (%d).",
		    __func__, sb, sb->sb_cc, toep->sb_cc));
		toep->rx_credits += toep->sb_cc - sb->sb_cc;
#ifdef USE_DDP_RX_FLOW_CONTROL
		toep->rx_credits -= m->m_len;	/* adjust for F_RX_FC_DDP */
#endif
		sbappendstream_locked(sb, m);
		toep->sb_cc = sb->sb_cc;
	}
	socantrcvmore_locked(so);	/* unlocks the sockbuf */

	KASSERT(tp->rcv_nxt == be32toh(cpl->rcv_nxt),
	    ("%s: rcv_nxt mismatch: %u %u", __func__, tp->rcv_nxt,
	    be32toh(cpl->rcv_nxt)));

	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
		tp->t_starttime = ticks;
		/* FALLTHROUGH */ 

	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_CLOSE_WAIT;
		break;

	case TCPS_FIN_WAIT_1:
		tp->t_state = TCPS_CLOSING;
		break;

	case TCPS_FIN_WAIT_2:
		tcp_twstart(tp);
		INP_UNLOCK_ASSERT(inp);	 /* safe, we have a ref on the inp */
		INP_INFO_WUNLOCK(&V_tcbinfo);

		INP_WLOCK(inp);
		final_cpl_received(toep);
		return (0);

	default:
		log(LOG_ERR, "%s: TID %u received CPL_PEER_CLOSE in state %d\n",
		    __func__, tid, tp->t_state);
	}
done:
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	return (0);
}

/*
 * Peer has ACK'd our FIN.
 */
static int
do_close_con_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_close_con_rpl *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = NULL;
	struct socket *so = NULL;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_CLOSE_CON_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	CTR4(KTR_CXGBE, "%s: tid %u (%s), toep_flags 0x%x",
	    __func__, tid, tp ? tcpstates[tp->t_state] : "no tp", toep->flags);

	if (toep->flags & TPF_ABORT_SHUTDOWN)
		goto done;

	so = inp->inp_socket;
	tp->snd_una = be32toh(cpl->snd_nxt) - 1;	/* exclude FIN */

	switch (tp->t_state) {
	case TCPS_CLOSING:	/* see TCPS_FIN_WAIT_2 in do_peer_close too */
		tcp_twstart(tp);
release:
		INP_UNLOCK_ASSERT(inp);	/* safe, we have a ref on the  inp */
		INP_INFO_WUNLOCK(&V_tcbinfo);

		INP_WLOCK(inp);
		final_cpl_received(toep);	/* no more CPLs expected */

		return (0);
	case TCPS_LAST_ACK:
		if (tcp_close(tp))
			INP_WUNLOCK(inp);
		goto release;

	case TCPS_FIN_WAIT_1:
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
			soisdisconnected(so);
		tp->t_state = TCPS_FIN_WAIT_2;
		break;

	default:
		log(LOG_ERR,
		    "%s: TID %u received CPL_CLOSE_CON_RPL in state %s\n",
		    __func__, tid, tcpstates[tp->t_state]);
	}
done:
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	return (0);
}

void
send_abort_rpl(struct adapter *sc, struct sge_wrq *ofld_txq, int tid,
    int rst_status)
{
	struct wrqe *wr;
	struct cpl_abort_rpl *cpl;

	wr = alloc_wrqe(sizeof(*cpl), ofld_txq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	cpl = wrtod(wr);

	INIT_TP_WR_MIT_CPL(cpl, CPL_ABORT_RPL, tid);
	cpl->cmd = rst_status;

	t4_wrq_tx(sc, wr);
}

static int
abort_status_to_errno(struct tcpcb *tp, unsigned int abort_reason)
{
	switch (abort_reason) {
	case CPL_ERR_BAD_SYN:
	case CPL_ERR_CONN_RESET:
		return (tp->t_state == TCPS_CLOSE_WAIT ? EPIPE : ECONNRESET);
	case CPL_ERR_XMIT_TIMEDOUT:
	case CPL_ERR_PERSIST_TIMEDOUT:
	case CPL_ERR_FINWAIT2_TIMEDOUT:
	case CPL_ERR_KEEPALIVE_TIMEDOUT:
		return (ETIMEDOUT);
	default:
		return (EIO);
	}
}

/*
 * TCP RST from the peer, timeout, or some other such critical error.
 */
static int
do_abort_req(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_req_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct sge_wrq *ofld_txq = toep->ofld_txq;
	struct inpcb *inp;
	struct tcpcb *tp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_REQ_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (toep->flags & TPF_SYNQE)
		return (do_abort_req_synqe(iq, rss, m));

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	if (negative_advice(cpl->status)) {
		CTR4(KTR_CXGBE, "%s: negative advice %d for tid %d (0x%x)",
		    __func__, cpl->status, tid, toep->flags);
		return (0);	/* Ignore negative advice */
	}

	inp = toep->inp;
	INP_INFO_WLOCK(&V_tcbinfo);	/* for tcp_close */
	INP_WLOCK(inp);

	tp = intotcpcb(inp);

	CTR6(KTR_CXGBE,
	    "%s: tid %d (%s), toep_flags 0x%x, inp_flags 0x%x, status %d",
	    __func__, tid, tp ? tcpstates[tp->t_state] : "no tp", toep->flags,
	    inp->inp_flags, cpl->status);

	/*
	 * If we'd initiated an abort earlier the reply to it is responsible for
	 * cleaning up resources.  Otherwise we tear everything down right here
	 * right now.  We owe the T4 a CPL_ABORT_RPL no matter what.
	 */
	if (toep->flags & TPF_ABORT_SHUTDOWN) {
		INP_WUNLOCK(inp);
		goto done;
	}
	toep->flags |= TPF_ABORT_SHUTDOWN;

	if ((inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) == 0) {
		struct socket *so = inp->inp_socket;

		if (so != NULL)
			so_error_set(so, abort_status_to_errno(tp,
			    cpl->status));
		tp = tcp_close(tp);
		if (tp == NULL)
			INP_WLOCK(inp);	/* re-acquire */
	}

	final_cpl_received(toep);
done:
	INP_INFO_WUNLOCK(&V_tcbinfo);
	send_abort_rpl(sc, ofld_txq, tid, CPL_ABORT_NO_RST);
	return (0);
}

/*
 * Reply to the CPL_ABORT_REQ (send_reset)
 */
static int
do_abort_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_rpl_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_RPL_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (toep->flags & TPF_SYNQE)
		return (do_abort_rpl_synqe(iq, rss, m));

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	CTR5(KTR_CXGBE, "%s: tid %u, toep %p, inp %p, status %d",
	    __func__, tid, toep, inp, cpl->status);

	KASSERT(toep->flags & TPF_ABORT_SHUTDOWN,
	    ("%s: wasn't expecting abort reply", __func__));

	INP_WLOCK(inp);
	final_cpl_received(toep);

	return (0);
}

static int
do_rx_data(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_data *cpl = mtod(m, const void *);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp;
	struct socket *so;
	struct sockbuf *sb;
	int len;
	uint32_t ddp_placed = 0;

	if (__predict_false(toep->flags & TPF_SYNQE)) {
#ifdef INVARIANTS
		struct synq_entry *synqe = (void *)toep;

		INP_WLOCK(synqe->lctx->inp);
		if (synqe->flags & TPF_SYNQE_HAS_L2TE) {
			KASSERT(synqe->flags & TPF_ABORT_SHUTDOWN,
			    ("%s: listen socket closed but tid %u not aborted.",
			    __func__, tid));
		} else {
			/*
			 * do_pass_accept_req is still running and will
			 * eventually take care of this tid.
			 */
		}
		INP_WUNLOCK(synqe->lctx->inp);
#endif
		CTR4(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x)", __func__, tid,
		    toep, toep->flags);
		m_freem(m);
		return (0);
	}

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));
	len = m->m_pkthdr.len;

	INP_WLOCK(inp);
	if (inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) {
		CTR4(KTR_CXGBE, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, len, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

	tp = intotcpcb(inp);

	if (__predict_false(tp->rcv_nxt != be32toh(cpl->seq)))
		ddp_placed = be32toh(cpl->seq) - tp->rcv_nxt;

	tp->rcv_nxt += len;
	KASSERT(tp->rcv_wnd >= len, ("%s: negative window size", __func__));
	tp->rcv_wnd -= len;
	tp->t_rcvtime = ticks;

	so = inp_inpcbtosocket(inp);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);

	if (__predict_false(sb->sb_state & SBS_CANTRCVMORE)) {
		CTR3(KTR_CXGBE, "%s: tid %u, excess rx (%d bytes)",
		    __func__, tid, len);
		m_freem(m);
		SOCKBUF_UNLOCK(sb);
		INP_WUNLOCK(inp);

		INP_INFO_WLOCK(&V_tcbinfo);
		INP_WLOCK(inp);
		tp = tcp_drop(tp, ECONNRESET);
		if (tp)
			INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);

		return (0);
	}

	/* receive buffer autosize */
	if (sb->sb_flags & SB_AUTOSIZE &&
	    V_tcp_do_autorcvbuf &&
	    sb->sb_hiwat < V_tcp_autorcvbuf_max &&
	    len > (sbspace(sb) / 8 * 7)) {
		unsigned int hiwat = sb->sb_hiwat;
		unsigned int newsize = min(hiwat + V_tcp_autorcvbuf_inc,
		    V_tcp_autorcvbuf_max);

		if (!sbreserve_locked(sb, newsize, so, NULL))
			sb->sb_flags &= ~SB_AUTOSIZE;
		else
			toep->rx_credits += newsize - hiwat;
	}

	if (toep->ulp_mode == ULP_MODE_TCPDDP) {
		int changed = !(toep->ddp_flags & DDP_ON) ^ cpl->ddp_off;

		if (changed) {
			if (toep->ddp_flags & DDP_SC_REQ)
				toep->ddp_flags ^= DDP_ON | DDP_SC_REQ;
			else {
				KASSERT(cpl->ddp_off == 1,
				    ("%s: DDP switched on by itself.",
				    __func__));

				/* Fell out of DDP mode */
				toep->ddp_flags &= ~(DDP_ON | DDP_BUF0_ACTIVE |
				    DDP_BUF1_ACTIVE);

				if (ddp_placed)
					insert_ddp_data(toep, ddp_placed);
			}
		}

		if ((toep->ddp_flags & DDP_OK) == 0 &&
		    time_uptime >= toep->ddp_disabled + DDP_RETRY_WAIT) {
			toep->ddp_score = DDP_LOW_SCORE;
			toep->ddp_flags |= DDP_OK;
			CTR3(KTR_CXGBE, "%s: tid %u DDP_OK @ %u",
			    __func__, tid, time_uptime);
		}

		if (toep->ddp_flags & DDP_ON) {

			/*
			 * CPL_RX_DATA with DDP on can only be an indicate.  Ask
			 * soreceive to post a buffer or disable DDP.  The
			 * payload that arrived in this indicate is appended to
			 * the socket buffer as usual.
			 */

#if 0
			CTR5(KTR_CXGBE,
			    "%s: tid %u (0x%x) DDP indicate (seq 0x%x, len %d)",
			    __func__, tid, toep->flags, be32toh(cpl->seq), len);
#endif
			sb->sb_flags |= SB_DDP_INDICATE;
		} else if ((toep->ddp_flags & (DDP_OK|DDP_SC_REQ)) == DDP_OK &&
		    tp->rcv_wnd > DDP_RSVD_WIN && len >= sc->tt.ddp_thres) {

			/*
			 * DDP allowed but isn't on (and a request to switch it
			 * on isn't pending either), and conditions are ripe for
			 * it to work.  Switch it on.
			 */

			enable_ddp(sc, toep);
		}
	}

	KASSERT(toep->sb_cc >= sb->sb_cc,
	    ("%s: sb %p has more data (%d) than last time (%d).",
	    __func__, sb, sb->sb_cc, toep->sb_cc));
	toep->rx_credits += toep->sb_cc - sb->sb_cc;
	sbappendstream_locked(sb, m);
	toep->sb_cc = sb->sb_cc;
	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(sb);

	INP_WUNLOCK(inp);
	return (0);
}

#define S_CPL_FW4_ACK_OPCODE    24
#define M_CPL_FW4_ACK_OPCODE    0xff
#define V_CPL_FW4_ACK_OPCODE(x) ((x) << S_CPL_FW4_ACK_OPCODE)
#define G_CPL_FW4_ACK_OPCODE(x) \
    (((x) >> S_CPL_FW4_ACK_OPCODE) & M_CPL_FW4_ACK_OPCODE)
 
#define S_CPL_FW4_ACK_FLOWID    0
#define M_CPL_FW4_ACK_FLOWID    0xffffff
#define V_CPL_FW4_ACK_FLOWID(x) ((x) << S_CPL_FW4_ACK_FLOWID)
#define G_CPL_FW4_ACK_FLOWID(x) \
    (((x) >> S_CPL_FW4_ACK_FLOWID) & M_CPL_FW4_ACK_FLOWID)
 
#define S_CPL_FW4_ACK_CR        24
#define M_CPL_FW4_ACK_CR        0xff
#define V_CPL_FW4_ACK_CR(x)     ((x) << S_CPL_FW4_ACK_CR)
#define G_CPL_FW4_ACK_CR(x)     (((x) >> S_CPL_FW4_ACK_CR) & M_CPL_FW4_ACK_CR)
 
#define S_CPL_FW4_ACK_SEQVAL    0
#define M_CPL_FW4_ACK_SEQVAL    0x1
#define V_CPL_FW4_ACK_SEQVAL(x) ((x) << S_CPL_FW4_ACK_SEQVAL)
#define G_CPL_FW4_ACK_SEQVAL(x) \
    (((x) >> S_CPL_FW4_ACK_SEQVAL) & M_CPL_FW4_ACK_SEQVAL)
#define F_CPL_FW4_ACK_SEQVAL    V_CPL_FW4_ACK_SEQVAL(1U)

static int
do_fw4_ack(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_fw4_ack *cpl = (const void *)(rss + 1);
	unsigned int tid = G_CPL_FW4_ACK_FLOWID(be32toh(OPCODE_TID(cpl)));
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp;
	struct tcpcb *tp;
	struct socket *so;
	uint8_t credits = cpl->credits;
	struct ofld_tx_sdesc *txsd;
	int plen;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_FW4_ACK_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	/*
	 * Very unusual case: we'd sent a flowc + abort_req for a synq entry and
	 * now this comes back carrying the credits for the flowc.
	 */
	if (__predict_false(toep->flags & TPF_SYNQE)) {
		KASSERT(toep->flags & TPF_ABORT_SHUTDOWN,
		    ("%s: credits for a synq entry %p", __func__, toep));
		return (0);
	}

	inp = toep->inp;

	KASSERT(opcode == CPL_FW4_ACK,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	INP_WLOCK(inp);

	if (__predict_false(toep->flags & TPF_ABORT_SHUTDOWN)) {
		INP_WUNLOCK(inp);
		return (0);
	}

	KASSERT((inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) == 0,
	    ("%s: inp_flags 0x%x", __func__, inp->inp_flags));

	tp = intotcpcb(inp);

	if (cpl->flags & CPL_FW4_ACK_FLAGS_SEQVAL) {
		tcp_seq snd_una = be32toh(cpl->snd_una);

#ifdef INVARIANTS
		if (__predict_false(SEQ_LT(snd_una, tp->snd_una))) {
			log(LOG_ERR,
			    "%s: unexpected seq# %x for TID %u, snd_una %x\n",
			    __func__, snd_una, toep->tid, tp->snd_una);
		}
#endif

		if (tp->snd_una != snd_una) {
			tp->snd_una = snd_una;
			tp->ts_recent_age = tcp_ts_getticks();
		}
	}

	so = inp->inp_socket;
	txsd = &toep->txsd[toep->txsd_cidx];
	plen = 0;
	while (credits) {
		KASSERT(credits >= txsd->tx_credits,
		    ("%s: too many (or partial) credits", __func__));
		credits -= txsd->tx_credits;
		toep->tx_credits += txsd->tx_credits;
		plen += txsd->plen;
		txsd++;
		toep->txsd_avail++;
		KASSERT(toep->txsd_avail <= toep->txsd_total,
		    ("%s: txsd avail > total", __func__));
		if (__predict_false(++toep->txsd_cidx == toep->txsd_total)) {
			txsd = &toep->txsd[0];
			toep->txsd_cidx = 0;
		}
	}

	if (plen > 0) {
		struct sockbuf *sb = &so->so_snd;

		SOCKBUF_LOCK(sb);
		sbdrop_locked(sb, plen);
		sowwakeup_locked(so);
		SOCKBUF_UNLOCK_ASSERT(sb);
	}

	/* XXX */
	if ((toep->flags & TPF_TX_SUSPENDED &&
	    toep->tx_credits >= MIN_OFLD_TX_CREDITS) ||
	    toep->tx_credits == toep->txsd_total *
	    howmany((sizeof(struct fw_ofld_tx_data_wr) + 1), 16)) {
		toep->flags &= ~TPF_TX_SUSPENDED;
		t4_push_frames(sc, toep);
	}
	INP_WUNLOCK(inp);

	return (0);
}

static int
do_set_tcb_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_set_tcb_rpl *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_SET_TCB_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (tid >= sc->tids.ftid_base &&
	    tid < sc->tids.ftid_base + sc->tids.nftids)
		return (t4_filter_rpl(iq, rss, m)); /* TCB is a filter */

	CXGBE_UNIMPLEMENTED(__func__);
}

void
t4_set_tcb_field(struct adapter *sc, struct toepcb *toep, uint16_t word,
    uint64_t mask, uint64_t val)
{
	struct wrqe *wr;
	struct cpl_set_tcb_field *req;

	wr = alloc_wrqe(sizeof(*req), toep->ctrlq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_SET_TCB_FIELD, toep->tid);
	req->reply_ctrl = htobe16(V_NO_REPLY(1) |
	    V_QUEUENO(toep->ofld_rxq->iq.abs_id));
	req->word_cookie = htobe16(V_WORD(word) | V_COOKIE(0));
	req->mask = htobe64(mask);
	req->val = htobe64(val);

	t4_wrq_tx(sc, wr);
}

void
t4_init_cpl_io_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_PEER_CLOSE, do_peer_close);
	t4_register_cpl_handler(sc, CPL_CLOSE_CON_RPL, do_close_con_rpl);
	t4_register_cpl_handler(sc, CPL_ABORT_REQ_RSS, do_abort_req);
	t4_register_cpl_handler(sc, CPL_ABORT_RPL_RSS, do_abort_rpl);
	t4_register_cpl_handler(sc, CPL_RX_DATA, do_rx_data);
	t4_register_cpl_handler(sc, CPL_FW4_ACK, do_fw4_ack);
	t4_register_cpl_handler(sc, CPL_SET_TCB_RPL, do_set_tcb_rpl);
}

void
t4_uninit_cpl_io_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_SET_TCB_RPL, t4_filter_rpl);
}
#endif
