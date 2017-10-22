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
#include <sys/refcount.h>
#include <sys/domain.h>
#include <sys/fnv_hash.h>
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

/* stid services */
static int alloc_stid(struct adapter *, void *);
static void *lookup_stid(struct adapter *, int);
static void free_stid(struct adapter *, int);

/* lctx services */
static struct listen_ctx *alloc_lctx(struct adapter *, struct inpcb *,
    struct port_info *);
static int free_lctx(struct adapter *, struct listen_ctx *);
static void hold_lctx(struct listen_ctx *);
static void listen_hash_add(struct adapter *, struct listen_ctx *);
static struct listen_ctx *listen_hash_find(struct adapter *, struct inpcb *);
static struct listen_ctx *listen_hash_del(struct adapter *, struct inpcb *);
static struct inpcb *release_lctx(struct adapter *, struct listen_ctx *);

static inline void save_qids_in_mbuf(struct mbuf *, struct port_info *);
static inline void get_qids_from_mbuf(struct mbuf *m, int *, int *);
static void send_reset_synqe(struct toedev *, struct synq_entry *);

/* XXX: won't work for IPv6 */
static int
alloc_stid(struct adapter *sc, void *ctx)
{
	struct tid_info *t = &sc->tids;
	int stid = -1;

	mtx_lock(&t->stid_lock);
	if (t->sfree) {
		union serv_entry *p = t->sfree;

		stid = p - t->stid_tab;
		stid += t->stid_base;
		t->sfree = p->next;
		p->data = ctx;
		t->stids_in_use++;
	}
	mtx_unlock(&t->stid_lock);
	return (stid);
}

static void *
lookup_stid(struct adapter *sc, int stid)
{
	struct tid_info *t = &sc->tids;

	return (t->stid_tab[stid - t->stid_base].data);
}

static void
free_stid(struct adapter *sc, int stid)
{
	struct tid_info *t = &sc->tids;
	union serv_entry *p = &t->stid_tab[stid - t->stid_base];

	mtx_lock(&t->stid_lock);
	p->next = t->sfree;
	t->sfree = p;
	t->stids_in_use--;
	mtx_unlock(&t->stid_lock);
}

static struct listen_ctx *
alloc_lctx(struct adapter *sc, struct inpcb *inp, struct port_info *pi)
{
	struct listen_ctx *lctx;

	INP_WLOCK_ASSERT(inp);

	lctx = malloc(sizeof(struct listen_ctx), M_CXGBE, M_NOWAIT | M_ZERO);
	if (lctx == NULL)
		return (NULL);

	lctx->stid = alloc_stid(sc, lctx);
	if (lctx->stid < 0) {
		free(lctx, M_CXGBE);
		return (NULL);
	}

	lctx->ctrlq = &sc->sge.ctrlq[pi->port_id];
	lctx->ofld_rxq = &sc->sge.ofld_rxq[pi->first_ofld_rxq];
	refcount_init(&lctx->refcount, 1);
	TAILQ_INIT(&lctx->synq);

	lctx->inp = inp;
	in_pcbref(inp);

	return (lctx);
}

/* Don't call this directly, use release_lctx instead */
static int
free_lctx(struct adapter *sc, struct listen_ctx *lctx)
{
	struct inpcb *inp = lctx->inp;

	INP_WLOCK_ASSERT(inp);
	KASSERT(lctx->refcount == 0,
	    ("%s: refcount %d", __func__, lctx->refcount));
	KASSERT(TAILQ_EMPTY(&lctx->synq),
	    ("%s: synq not empty.", __func__));
	KASSERT(lctx->stid >= 0, ("%s: bad stid %d.", __func__, lctx->stid));

	CTR4(KTR_CXGBE, "%s: stid %u, lctx %p, inp %p",
	    __func__, lctx->stid, lctx, lctx->inp);

	free_stid(sc, lctx->stid);
	free(lctx, M_CXGBE);

	return (in_pcbrele_wlocked(inp));
}

static void
hold_lctx(struct listen_ctx *lctx)
{

	refcount_acquire(&lctx->refcount);
}

static inline uint32_t
listen_hashfn(void *key, u_long mask)
{

	return (fnv_32_buf(&key, sizeof(key), FNV1_32_INIT) & mask);
}

/*
 * Add a listen_ctx entry to the listen hash table.
 */
static void
listen_hash_add(struct adapter *sc, struct listen_ctx *lctx)
{
	struct tom_data *td = sc->tom_softc;
	int bucket = listen_hashfn(lctx->inp, td->listen_mask);

	mtx_lock(&td->lctx_hash_lock);
	LIST_INSERT_HEAD(&td->listen_hash[bucket], lctx, link);
	td->lctx_count++;
	mtx_unlock(&td->lctx_hash_lock);
}

/*
 * Look for the listening socket's context entry in the hash and return it.
 */
static struct listen_ctx *
listen_hash_find(struct adapter *sc, struct inpcb *inp)
{
	struct tom_data *td = sc->tom_softc;
	int bucket = listen_hashfn(inp, td->listen_mask);
	struct listen_ctx *lctx;

	mtx_lock(&td->lctx_hash_lock);
	LIST_FOREACH(lctx, &td->listen_hash[bucket], link) {
		if (lctx->inp == inp)
			break;
	}
	mtx_unlock(&td->lctx_hash_lock);

	return (lctx);
}

/*
 * Removes the listen_ctx structure for inp from the hash and returns it.
 */
static struct listen_ctx *
listen_hash_del(struct adapter *sc, struct inpcb *inp)
{
	struct tom_data *td = sc->tom_softc;
	int bucket = listen_hashfn(inp, td->listen_mask);
	struct listen_ctx *lctx, *l;

	mtx_lock(&td->lctx_hash_lock);
	LIST_FOREACH_SAFE(lctx, &td->listen_hash[bucket], link, l) {
		if (lctx->inp == inp) {
			LIST_REMOVE(lctx, link);
			td->lctx_count--;
			break;
		}
	}
	mtx_unlock(&td->lctx_hash_lock);

	return (lctx);
}

/*
 * Releases a hold on the lctx.  Must be called with the listening socket's inp
 * locked.  The inp may be freed by this function and it returns NULL to
 * indicate this.
 */
static struct inpcb *
release_lctx(struct adapter *sc, struct listen_ctx *lctx)
{
	struct inpcb *inp = lctx->inp;
	int inp_freed = 0;

	INP_WLOCK_ASSERT(inp);
	if (refcount_release(&lctx->refcount))
		inp_freed = free_lctx(sc, lctx);

	return (inp_freed ? NULL : inp);
}

static void
send_reset_synqe(struct toedev *tod, struct synq_entry *synqe)
{
	struct adapter *sc = tod->tod_softc;
	struct mbuf *m = synqe->syn;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct port_info *pi = ifp->if_softc;
	struct l2t_entry *e = &sc->l2t->l2tab[synqe->l2e_idx];
        struct wrqe *wr;
        struct fw_flowc_wr *flowc;
	struct cpl_abort_req *req;
	int txqid, rxqid, flowclen;
	struct sge_wrq *ofld_txq;
	struct sge_ofld_rxq *ofld_rxq;
	const int nparams = 4;
	unsigned int pfvf = G_FW_VIID_PFN(pi->viid) << S_FW_VIID_PFN;

	INP_WLOCK_ASSERT(synqe->lctx->inp);

	CTR4(KTR_CXGBE, "%s: synqe %p, tid %d%s",
	    __func__, synqe, synqe->tid,
	    synqe_flag(synqe, TPF_ABORT_SHUTDOWN) ?
	    " (abort already in progress)" : "");
	if (synqe_flag(synqe, TPF_ABORT_SHUTDOWN))
		return;	/* abort already in progress */
	synqe_set_flag(synqe, TPF_ABORT_SHUTDOWN);

	get_qids_from_mbuf(m, &txqid, &rxqid);
	ofld_txq = &sc->sge.ofld_txq[txqid];
	ofld_rxq = &sc->sge.ofld_rxq[rxqid];

	/* The wrqe will have two WRs - a flowc followed by an abort_req */
	flowclen = sizeof(*flowc) + nparams * sizeof(struct fw_flowc_mnemval);

	wr = alloc_wrqe(roundup(flowclen, EQ_ESIZE) + sizeof(*req), ofld_txq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	flowc = wrtod(wr);
	req = (void *)((caddr_t)flowc + roundup(flowclen, EQ_ESIZE));

	/* First the flowc ... */
	memset(flowc, 0, wr->wr_len);
	flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
	    V_FW_FLOWC_WR_NPARAMS(nparams));
	flowc->flowid_len16 = htonl(V_FW_WR_LEN16(howmany(flowclen, 16)) |
	    V_FW_WR_FLOWID(synqe->tid));
	flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_PFNVFN;
        flowc->mnemval[0].val = htobe32(pfvf);
        flowc->mnemval[1].mnemonic = FW_FLOWC_MNEM_CH;
        flowc->mnemval[1].val = htobe32(pi->tx_chan);
        flowc->mnemval[2].mnemonic = FW_FLOWC_MNEM_PORT;
        flowc->mnemval[2].val = htobe32(pi->tx_chan);
        flowc->mnemval[3].mnemonic = FW_FLOWC_MNEM_IQID;
        flowc->mnemval[3].val = htobe32(ofld_rxq->iq.abs_id);
	synqe_set_flag(synqe, TPF_FLOWC_WR_SENT);

	/* ... then ABORT request */
	INIT_TP_WR_MIT_CPL(req, CPL_ABORT_REQ, synqe->tid);
	req->rsvd0 = 0;	/* don't have a snd_nxt */
	req->rsvd1 = 1;	/* no data sent yet */
	req->cmd = CPL_ABORT_SEND_RST;

	t4_l2t_send(sc, wr, e);
}

static int
create_server(struct adapter *sc, struct listen_ctx *lctx)
{
	struct wrqe *wr;
	struct cpl_pass_open_req *req;
	struct in_conninfo *inc = &lctx->inp->inp_inc;

	wr = alloc_wrqe(sizeof(*req), lctx->ctrlq);
	if (wr == NULL) {
		log(LOG_ERR, "%s: allocation failure", __func__);
		return (ENOMEM);
	}
	req = wrtod(wr);

	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htobe32(MK_OPCODE_TID(CPL_PASS_OPEN_REQ, lctx->stid));
	req->local_port = inc->inc_lport;
	req->peer_port = 0;
	req->local_ip = inc->inc_laddr.s_addr;
	req->peer_ip = 0;
	req->opt0 = htobe64(V_TX_CHAN(lctx->ctrlq->eq.tx_chan));
	req->opt1 = htobe64(V_CONN_POLICY(CPL_CONN_POLICY_ASK) |
	    F_SYN_RSS_ENABLE | V_SYN_RSS_QUEUE(lctx->ofld_rxq->iq.abs_id));

	t4_wrq_tx(sc, wr);
	return (0);
}

static int
destroy_server(struct adapter *sc, struct listen_ctx *lctx)
{
	struct wrqe *wr;
	struct cpl_close_listsvr_req *req;

	wr = alloc_wrqe(sizeof(*req), lctx->ctrlq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_LISTSRV_REQ,
	    lctx->stid));
	req->reply_ctrl = htobe16(lctx->ofld_rxq->iq.abs_id);
	req->rsvd = htobe16(0);

	t4_wrq_tx(sc, wr);
	return (0);
}

/*
 * Start a listening server by sending a passive open request to HW.
 *
 * Can't take adapter lock here and access to sc->flags, sc->open_device_map,
 * sc->offload_map, if_capenable are all race prone.
 */
int
t4_listen_start(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
	struct port_info *pi;
	struct inpcb *inp = tp->t_inpcb;
	struct listen_ctx *lctx;
	int i;

	INP_WLOCK_ASSERT(inp);

	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (0);

#if 0
	ADAPTER_LOCK(sc);
	if (IS_BUSY(sc)) {
		log(LOG_ERR, "%s: listen request ignored, %s is busy",
		    __func__, device_get_nameunit(sc->dev));
		goto done;
	}

	KASSERT(sc->flags & TOM_INIT_DONE,
	    ("%s: TOM not initialized", __func__));
#endif

	if ((sc->open_device_map & sc->offload_map) == 0)
		goto done;	/* no port that's UP with IFCAP_TOE enabled */

	/*
	 * Find a running port with IFCAP_TOE4.  We'll use the first such port's
	 * queues to send the passive open and receive the reply to it.
	 *
	 * XXX: need a way to mark a port in use by offload.  if_cxgbe should
	 * then reject any attempt to bring down such a port (and maybe reject
	 * attempts to disable IFCAP_TOE on that port too?).
	 */
	for_each_port(sc, i) {
		if (isset(&sc->open_device_map, i) &&
		    sc->port[i]->ifp->if_capenable & IFCAP_TOE4)
				break;
	}
	KASSERT(i < sc->params.nports,
	    ("%s: no running port with TOE capability enabled.", __func__));
	pi = sc->port[i];

	if (listen_hash_find(sc, inp) != NULL)
		goto done;	/* already setup */

	lctx = alloc_lctx(sc, inp, pi);
	if (lctx == NULL) {
		log(LOG_ERR,
		    "%s: listen request ignored, %s couldn't allocate lctx\n",
		    __func__, device_get_nameunit(sc->dev));
		goto done;
	}
	listen_hash_add(sc, lctx);

	CTR5(KTR_CXGBE, "%s: stid %u (%s), lctx %p, inp %p", __func__,
	    lctx->stid, tcpstates[tp->t_state], lctx, inp);

	if (create_server(sc, lctx) != 0) {
		log(LOG_ERR, "%s: %s failed to create hw listener.\n", __func__,
		    device_get_nameunit(sc->dev));
		(void) listen_hash_del(sc, inp);
		inp = release_lctx(sc, lctx);
		/* can't be freed, host stack has a reference */
		KASSERT(inp != NULL, ("%s: inp freed", __func__));
		goto done;
	}
	lctx->flags |= LCTX_RPL_PENDING;
done:
#if 0
	ADAPTER_UNLOCK(sc);
#endif
	return (0);
}

int
t4_listen_stop(struct toedev *tod, struct tcpcb *tp)
{
	struct listen_ctx *lctx;
	struct adapter *sc = tod->tod_softc;
	struct inpcb *inp = tp->t_inpcb;
	struct synq_entry *synqe;

	INP_WLOCK_ASSERT(inp);

	lctx = listen_hash_del(sc, inp);
	if (lctx == NULL)
		return (ENOENT);	/* no hardware listener for this inp */

	CTR4(KTR_CXGBE, "%s: stid %u, lctx %p, flags %x", __func__, lctx->stid,
	    lctx, lctx->flags);

	/*
	 * If the reply to the PASS_OPEN is still pending we'll wait for it to
	 * arrive and clean up when it does.
	 */
	if (lctx->flags & LCTX_RPL_PENDING) {
		KASSERT(TAILQ_EMPTY(&lctx->synq),
		    ("%s: synq not empty.", __func__));
		return (EINPROGRESS);
	}

	/*
	 * The host stack will abort all the connections on the listening
	 * socket's so_comp.  It doesn't know about the connections on the synq
	 * so we need to take care of those.
	 */
	TAILQ_FOREACH(synqe, &lctx->synq, link)
		send_reset_synqe(tod, synqe);

	destroy_server(sc, lctx);
	return (0);
}

static inline void
hold_synqe(struct synq_entry *synqe)
{

	refcount_acquire(&synqe->refcnt);
}

static inline void
release_synqe(struct synq_entry *synqe)
{

	if (refcount_release(&synqe->refcnt)) {
		int needfree = synqe_flag(synqe, TPF_SYNQE_NEEDFREE);

		m_freem(synqe->syn);
		if (needfree)
			free(synqe, M_CXGBE);
	}
}

void
t4_syncache_added(struct toedev *tod __unused, void *arg)
{
	struct synq_entry *synqe = arg;

	hold_synqe(synqe);
}

void
t4_syncache_removed(struct toedev *tod __unused, void *arg)
{
	struct synq_entry *synqe = arg;

	release_synqe(synqe);
}

/* XXX */
extern void tcp_dooptions(struct tcpopt *, u_char *, int, int);

int
t4_syncache_respond(struct toedev *tod, void *arg, struct mbuf *m)
{
	struct adapter *sc = tod->tod_softc;
	struct synq_entry *synqe = arg;
	struct wrqe *wr;
	struct l2t_entry *e;
	struct tcpopt to;
	struct ip *ip = mtod(m, struct ip *);
	struct tcphdr *th = (void *)(ip + 1);

	wr = (struct wrqe *)atomic_readandclear_ptr(&synqe->wr);
	if (wr == NULL)
		return (EALREADY);

	bzero(&to, sizeof(to));
	tcp_dooptions(&to, (void *)(th + 1), (th->th_off << 2) - sizeof(*th),
	    TO_SYN);

	/* save these for later */
	synqe->iss = be32toh(th->th_seq);
	synqe->ts = to.to_tsval;

	e = &sc->l2t->l2tab[synqe->l2e_idx];
	t4_l2t_send(sc, wr, e);

	m_freem(m);	/* don't need this any more */
	return (0);
}

static int
do_pass_open_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_pass_open_rpl *cpl = (const void *)(rss + 1);
	int stid = GET_TID(cpl);
	unsigned int status = cpl->status;
	struct listen_ctx *lctx = lookup_stid(sc, stid);
	struct inpcb *inp = lctx->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PASS_OPEN_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));

	INP_WLOCK(inp);

	CTR4(KTR_CXGBE, "%s: stid %d, status %u, flags 0x%x",
	    __func__, stid, status, lctx->flags);

	lctx->flags &= ~LCTX_RPL_PENDING;

	if (status != CPL_ERR_NONE)
		log(LOG_ERR, "listener with stid %u failed: %d", stid, status);

#ifdef INVARIANTS
	/*
	 * If the inp has been dropped (listening socket closed) then
	 * listen_stop must have run and taken the inp out of the hash.
	 */
	if (inp->inp_flags & INP_DROPPED) {
		KASSERT(listen_hash_del(sc, inp) == NULL,
		    ("%s: inp %p still in listen hash", __func__, inp));
	}
#endif

	if (inp->inp_flags & INP_DROPPED && status != CPL_ERR_NONE) {
		if (release_lctx(sc, lctx) != NULL)
			INP_WUNLOCK(inp);
		return (status);
	}

	/*
	 * Listening socket stopped listening earlier and now the chip tells us
	 * it has started the hardware listener.  Stop it; the lctx will be
	 * released in do_close_server_rpl.
	 */
	if (inp->inp_flags & INP_DROPPED) {
		destroy_server(sc, lctx);
		INP_WUNLOCK(inp);
		return (status);
	}

	/*
	 * Failed to start hardware listener.  Take inp out of the hash and
	 * release our reference on it.  An error message has been logged
	 * already.
	 */
	if (status != CPL_ERR_NONE) {
		listen_hash_del(sc, inp);
		if (release_lctx(sc, lctx) != NULL)
			INP_WUNLOCK(inp);
		return (status);
	}

	/* hardware listener open for business */

	INP_WUNLOCK(inp);
	return (status);
}

static int
do_close_server_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_close_listsvr_rpl *cpl = (const void *)(rss + 1);
	int stid = GET_TID(cpl);
	unsigned int status = cpl->status;
	struct listen_ctx *lctx = lookup_stid(sc, stid);
	struct inpcb *inp = lctx->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_CLOSE_LISTSRV_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));

	CTR3(KTR_CXGBE, "%s: stid %u, status %u", __func__, stid, status);

	if (status != CPL_ERR_NONE) {
		log(LOG_ERR, "%s: failed (%u) to close listener for stid %u",
		    __func__, status, stid);
		return (status);
	}

	INP_WLOCK(inp);
	inp = release_lctx(sc, lctx);
	if (inp != NULL)
		INP_WUNLOCK(inp);

	return (status);
}

static void
done_with_synqe(struct adapter *sc, struct synq_entry *synqe)
{
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
	struct port_info *pi = synqe->syn->m_pkthdr.rcvif->if_softc;
	struct l2t_entry *e = &sc->l2t->l2tab[synqe->l2e_idx];

	INP_WLOCK_ASSERT(inp);

	TAILQ_REMOVE(&lctx->synq, synqe, link);
	inp = release_lctx(sc, lctx);
	if (inp)
		INP_WUNLOCK(inp);
	remove_tid(sc, synqe->tid);
	release_tid(sc, synqe->tid, &sc->sge.ctrlq[pi->port_id]);
	t4_l2t_release(e);
	release_synqe(synqe);	/* removed from synq list */
}

int
do_abort_req_synqe(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_req_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(sc, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
	int txqid;
	struct sge_wrq *ofld_txq;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_REQ_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(synqe->tid == tid, ("%s: toep tid mismatch", __func__));

	CTR6(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x), lctx %p, status %d",
	    __func__, tid, synqe, synqe->flags, synqe->lctx, cpl->status);

	if (cpl->status == CPL_ERR_RTX_NEG_ADVICE ||
	    cpl->status == CPL_ERR_PERSIST_NEG_ADVICE)
		return (0);	/* Ignore negative advice */

	INP_WLOCK(inp);

	get_qids_from_mbuf(synqe->syn, &txqid, NULL);
	ofld_txq = &sc->sge.ofld_txq[txqid];

	/*
	 * If we'd initiated an abort earlier the reply to it is responsible for
	 * cleaning up resources.  Otherwise we tear everything down right here
	 * right now.  We owe the T4 a CPL_ABORT_RPL no matter what.
	 */
	if (synqe_flag(synqe, TPF_ABORT_SHUTDOWN)) {
		INP_WUNLOCK(inp);
		goto done;
	}

	done_with_synqe(sc, synqe);
	/* inp lock released by done_with_synqe */
done:
	send_abort_rpl(sc, ofld_txq, tid, CPL_ABORT_NO_RST);
	return (0);
}

int
do_abort_rpl_synqe(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_rpl_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(sc, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_RPL_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(synqe->tid == tid, ("%s: toep tid mismatch", __func__));

	CTR6(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x), lctx %p, status %d",
	    __func__, tid, synqe, synqe->flags, synqe->lctx, cpl->status);

	INP_WLOCK(inp);
	KASSERT(synqe_flag(synqe, TPF_ABORT_SHUTDOWN),
	    ("%s: wasn't expecting abort reply for synqe %p (0x%x)",
	    __func__, synqe, synqe->flags));

	done_with_synqe(sc, synqe);
	/* inp lock released by done_with_synqe */

	return (0);
}

void
t4_offload_socket(struct toedev *tod, void *arg, struct socket *so)
{
	struct adapter *sc = tod->tod_softc;
	struct synq_entry *synqe = arg;
#ifdef INVARIANTS
	struct inpcb *inp = sotoinpcb(so);
#endif
	struct cpl_pass_establish *cpl = mtod(synqe->syn, void *);
	struct toepcb *toep = *(struct toepcb **)(cpl + 1);

	INP_INFO_LOCK_ASSERT(&V_tcbinfo); /* prevents bad race with accept() */
	INP_WLOCK_ASSERT(inp);
	KASSERT(synqe_flag(synqe, TPF_SYNQE),
	    ("%s: %p not a synq_entry?", __func__, arg));

	offload_socket(so, toep);
	make_established(toep, cpl->snd_isn, cpl->rcv_isn, cpl->tcp_opt);
	toepcb_set_flag(toep, TPF_CPL_PENDING);
	update_tid(sc, synqe->tid, toep);
}

static inline void
save_qids_in_mbuf(struct mbuf *m, struct port_info *pi)
{
	uint32_t txqid, rxqid;

	txqid = (arc4random() % pi->nofldtxq) + pi->first_ofld_txq;
	rxqid = (arc4random() % pi->nofldrxq) + pi->first_ofld_rxq;

	m->m_pkthdr.flowid = (txqid << 16) | (rxqid & 0xffff);
}

static inline void
get_qids_from_mbuf(struct mbuf *m, int *txqid, int *rxqid)
{

	if (txqid)
		*txqid = m->m_pkthdr.flowid >> 16;
	if (rxqid)
		*rxqid = m->m_pkthdr.flowid & 0xffff;
}

/*
 * Use the trailing space in the mbuf in which the PASS_ACCEPT_REQ arrived to
 * store some state temporarily.
 */
static struct synq_entry *
mbuf_to_synqe(struct mbuf *m)
{
	int len = roundup(sizeof (struct synq_entry), 8);
	int tspace = M_TRAILINGSPACE(m);
	struct synq_entry *synqe = NULL;

	if (tspace < len) {
		synqe = malloc(sizeof(*synqe), M_CXGBE, M_NOWAIT);
		if (synqe == NULL)
			return (NULL);
	} else
		synqe = (void *)(m->m_data + m->m_len + tspace - sizeof(*synqe));

	synqe->flags = 0;
	synqe_set_flag(synqe, TPF_SYNQE);
	if (tspace < len)
		synqe_set_flag(synqe, TPF_SYNQE_NEEDFREE);

	return (synqe);
}

static void
t4opt_to_tcpopt(const struct tcp_options *t4opt, struct tcpopt *to)
{
	bzero(to, sizeof(*to));

	if (t4opt->mss) {
		to->to_flags |= TOF_MSS;
		to->to_mss = be16toh(t4opt->mss);
	}

	if (t4opt->wsf) {
		to->to_flags |= TOF_SCALE;
		to->to_wscale = t4opt->wsf;
	}

	if (t4opt->tstamp)
		to->to_flags |= TOF_TS;

	if (t4opt->sack)
		to->to_flags |= TOF_SACKPERM;
}

/*
 * Options2 for passive open.
 */
static uint32_t
calc_opt2p(struct adapter *sc, struct port_info *pi, int rxqid,
    const struct tcp_options *tcpopt, struct tcphdr *th)
{
	uint32_t opt2 = 0;
	struct sge_ofld_rxq *ofld_rxq = &sc->sge.ofld_rxq[rxqid];

	if (V_tcp_do_rfc1323) {
		if (tcpopt->tstamp)
			opt2 |= F_TSTAMPS_EN;
		if (tcpopt->sack)
			opt2 |= F_SACK_EN;
		if (tcpopt->wsf > 0)
			opt2 |= F_WND_SCALE_EN;
	}

	if (V_tcp_do_ecn && th->th_flags & (TH_ECE | TH_CWR))
		opt2 |= F_CCTRL_ECN;

	opt2 |= V_TX_QUEUE(sc->params.tp.tx_modq[pi->tx_chan]);
	opt2 |= F_RX_COALESCE_VALID | V_RX_COALESCE(M_RX_COALESCE);
	opt2 |= F_RSS_QUEUE_VALID | V_RSS_QUEUE(ofld_rxq->iq.abs_id);

	return htobe32(opt2);
}

/* XXX: duplication. */
static inline void
tcp_fields_to_host(struct tcphdr *th)
{

	th->th_seq = ntohl(th->th_seq);
	th->th_ack = ntohl(th->th_ack);
	th->th_win = ntohs(th->th_win);
	th->th_urp = ntohs(th->th_urp);
}

static void
pass_accept_req_to_protohdrs(const struct mbuf *m, struct in_conninfo *inc,
    struct tcphdr *th)
{
	const struct cpl_pass_accept_req *cpl = mtod(m, const void *);
	const struct ether_header *eh;
	unsigned int hlen = be32toh(cpl->hdr_len);
	const struct ip *ip;
	const struct tcphdr *tcp;

	eh = (const void *)(cpl + 1);
	ip = (const void *)((uintptr_t)eh + G_ETH_HDR_LEN(hlen));
	tcp = (const void *)((uintptr_t)ip + G_IP_HDR_LEN(hlen));

	if (inc) {
		bzero(inc, sizeof(*inc));
		inc->inc_faddr = ip->ip_src;
		inc->inc_laddr = ip->ip_dst;
		inc->inc_fport = tcp->th_sport;
		inc->inc_lport = tcp->th_dport;
		if (ip->ip_v == 6)
			inc->inc_flags |= INC_ISIPV6;
	}

	if (th) {
		bcopy(tcp, th, sizeof(*th));
		tcp_fields_to_host(th);		/* just like tcp_input */
	}
}

#define REJECT_PASS_ACCEPT()	do { \
	reject_reason = __LINE__; \
	goto reject; \
} while (0)

/*
 * The context associated with a tid entry via insert_tid could be a synq_entry
 * or a toepcb.  The only way CPL handlers can tell is via a bit in these flags.
 */
CTASSERT(offsetof(struct toepcb, flags) == offsetof(struct synq_entry, flags));

/*
 * Incoming SYN on a listening socket.
 *
 * XXX: Every use of ifp in this routine has a bad race with up/down, toe/-toe,
 * etc.
 */
static int
do_pass_accept_req(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct toedev *tod;
	const struct cpl_pass_accept_req *cpl = mtod(m, const void *);
	struct cpl_pass_accept_rpl *rpl;
	struct wrqe *wr;
	unsigned int stid = G_PASS_OPEN_TID(be32toh(cpl->tos_stid));
	unsigned int tid = GET_TID(cpl);
	struct listen_ctx *lctx = lookup_stid(sc, stid);
	struct inpcb *inp;
	struct socket *so;
	struct in_conninfo inc;
	struct tcphdr th;
	struct tcpopt to;
	struct port_info *pi;
	struct ifnet *ifp, *ifp_vlan = NULL;
	struct l2t_entry *e = NULL;
	struct rtentry *rt;
	struct sockaddr_in nam;
	int rscale, mtu_idx, rx_credits, rxqid;
	struct synq_entry *synqe = NULL;
	int reject_reason;
	uint16_t vid;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PASS_ACCEPT_REQ,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));

	CTR4(KTR_CXGBE, "%s: stid %u, tid %u, lctx %p", __func__, stid, tid,
	    lctx);

	pass_accept_req_to_protohdrs(m, &inc, &th);
	t4opt_to_tcpopt(&cpl->tcpopt, &to);

	pi = sc->port[G_SYN_INTF(be16toh(cpl->l2info))];
	ifp = pi->ifp;
	m->m_pkthdr.rcvif = ifp;
	tod = TOEDEV(ifp);

	/*
	 * Don't offload if the interface that received the SYN doesn't have
	 * IFCAP_TOE enabled.
	 */
	if ((ifp->if_capenable & IFCAP_TOE4) == 0)
		REJECT_PASS_ACCEPT();

	/* Don't offload IPv6 connections. XXX: add IPv6 support */
	if (inc.inc_flags & INC_ISIPV6)
		REJECT_PASS_ACCEPT();

	/*
	 * Don't offload if the SYN had a VLAN tag and the vid doesn't match
	 * anything on this interface.
	 */
	vid = EVL_VLANOFTAG(be16toh(cpl->vlan));
	if (vid != 0xfff) {
		ifp_vlan = VLAN_DEVAT(ifp, vid);
		if (ifp_vlan == NULL)
			REJECT_PASS_ACCEPT();
	}

	/*
	 * Don't offload if the peer requested a TCP option that's not known to
	 * the silicon.
	 */
	if (cpl->tcpopt.unknown)
		REJECT_PASS_ACCEPT();

	/*
	 * Don't offload if the outgoing interface for the route back to the
	 * peer is not the same as the interface that received the SYN.
	 * XXX: too restrictive.
	 */
	nam.sin_len = sizeof(nam);
	nam.sin_family = AF_INET;
	nam.sin_addr = inc.inc_faddr;
	rt = rtalloc1((struct sockaddr *)&nam, 0, 0);
	if (rt == NULL)
		REJECT_PASS_ACCEPT();
	else {
		struct sockaddr *nexthop;

		RT_UNLOCK(rt);
		nexthop = rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway :
		    (struct sockaddr *)&nam;
		if (rt->rt_ifp == ifp ||
		    (ifp_vlan != NULL && rt->rt_ifp == ifp_vlan))
			e = t4_l2t_get(pi, rt->rt_ifp, nexthop);
		RTFREE(rt);
		if (e == NULL)
			REJECT_PASS_ACCEPT();	/* no l2te, or ifp mismatch */
	}

	synqe = mbuf_to_synqe(m);
	if (synqe == NULL)
		REJECT_PASS_ACCEPT();

	wr = alloc_wrqe(sizeof(*rpl), &sc->sge.ctrlq[pi->port_id]);
	if (wr == NULL)
		REJECT_PASS_ACCEPT();
	rpl = wrtod(wr);

	INP_INFO_WLOCK(&V_tcbinfo);	/* for 4-tuple check, syncache_add */

	/* Don't offload if the 4-tuple is already in use */
	if (toe_4tuple_check(&inc, &th, ifp) != 0) {
		INP_INFO_WUNLOCK(&V_tcbinfo);
		free(wr, M_CXGBE);
		REJECT_PASS_ACCEPT();
	}

	inp = lctx->inp;		/* listening socket, not owned by TOE */
	INP_WLOCK(inp);

	/* Don't offload if the listening socket has closed */
	if (__predict_false(inp->inp_flags & INP_DROPPED)) {
		/*
		 * The listening socket has closed.  The reply from the TOE to
		 * our CPL_CLOSE_LISTSRV_REQ will ultimately release all
		 * resources tied to this listen context.
		 */
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		free(wr, M_CXGBE);
		REJECT_PASS_ACCEPT();
	}
	so = inp->inp_socket;

	mtu_idx = find_best_mtu_idx(sc, &inc, be16toh(cpl->tcpopt.mss));
	rscale = cpl->tcpopt.wsf && V_tcp_do_rfc1323 ? select_rcv_wscale() : 0;
	SOCKBUF_LOCK(&so->so_rcv);
	/* opt0 rcv_bufsiz initially, assumes its normal meaning later */
	rx_credits = min(select_rcv_wnd(so) >> 10, M_RCV_BUFSIZ);
	SOCKBUF_UNLOCK(&so->so_rcv);

	save_qids_in_mbuf(m, pi);
	get_qids_from_mbuf(m, NULL, &rxqid);

	INIT_TP_WR_MIT_CPL(rpl, CPL_PASS_ACCEPT_RPL, tid);
	rpl->opt0 = calc_opt0(so, pi, e, mtu_idx, rscale, rx_credits,
	    ULP_MODE_NONE);
	rpl->opt2 = calc_opt2p(sc, pi, rxqid, &cpl->tcpopt, &th);

	synqe->tid = tid;
	synqe->lctx = lctx;
	synqe->syn = m;
	m = NULL;
	refcount_init(&synqe->refcnt, 1); /* 1 so that it is held for the
					     duration of this function */
	synqe->l2e_idx = e->idx;
	synqe->rcv_bufsize = rx_credits;
	atomic_store_rel_ptr(&synqe->wr, (uintptr_t)wr);

	insert_tid(sc, tid, synqe);
	TAILQ_INSERT_TAIL(&lctx->synq, synqe, link);
	hold_synqe(synqe);	/* hold for the duration it's in the synq */
	hold_lctx(lctx);	/* A synqe on the list has a ref on its lctx */

	/*
	 * If all goes well t4_syncache_respond will get called during
	 * syncache_add.  Also note that syncache_add releases both pcbinfo and
	 * pcb locks.
	 */
	toe_syncache_add(&inc, &to, &th, inp, tod, synqe);
	INP_UNLOCK_ASSERT(inp);	/* ok to assert, we have a ref on the inp */
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);

	/*
	 * If we replied during syncache_add (synqe->wr has been consumed),
	 * good.  Otherwise, set it to 0 so that further syncache_respond
	 * attempts by the kernel will be ignored.
	 *
	 * The extra hold on the synqe makes sure that it is still around, even
	 * if the listener has been dropped and the synqe was aborted and the
	 * reply to the abort has removed and released the synqe from the synq
	 * list.
	 */
	if (atomic_cmpset_ptr(&synqe->wr, (uintptr_t)wr, 0)) {

		INP_WLOCK(inp);
		if (__predict_false(inp->inp_flags & INP_DROPPED)) {
			/* listener closed.  synqe must have been aborted. */
			KASSERT(synqe_flag(synqe, TPF_ABORT_SHUTDOWN),
			    ("%s: listener %p closed but synqe %p not aborted",
			    __func__, inp, synqe));

			CTR5(KTR_CXGBE,
			    "%s: stid %u, tid %u, lctx %p, synqe %p, ABORTED",
			    __func__, stid, tid, lctx, synqe);
			INP_WUNLOCK(inp);
			free(wr, M_CXGBE);
			release_synqe(synqe);	/* about to exit function */
			return (__LINE__);
		}

		/*
		 * synqe aborted before TOM replied to PASS_ACCEPT_REQ.  But
		 * that can only happen if the listener was closed and we just
		 * checked for that.
		 */
		KASSERT(!synqe_flag(synqe, TPF_ABORT_SHUTDOWN),
		    ("%s: synqe %p aborted, but listener %p not dropped.",
		    __func__, synqe, inp));

		/* Yank the synqe out of the lctx synq. */
		TAILQ_REMOVE(&lctx->synq, synqe, link);
		release_synqe(synqe);	/* removed from synq list */
		inp = release_lctx(sc, lctx);
		if (inp)
			INP_WUNLOCK(inp);

		/*
		 * syncache may or may not have a hold on the synqe, which may
		 * or may not be stashed in the original SYN mbuf passed to us.
		 * Just copy it over instead of dealing with all possibilities.
		 */
		m = m_dup(synqe->syn, M_DONTWAIT);
		if (m)
			m->m_pkthdr.rcvif = ifp;

		release_synqe(synqe);	/* about to exit function */
		free(wr, M_CXGBE);
		REJECT_PASS_ACCEPT();
	}
	release_synqe(synqe);	/* about to exit function */
	CTR5(KTR_CXGBE, "%s: stid %u, tid %u, lctx %p, synqe %p, SYNACK",
	    __func__, stid, tid, lctx, synqe);
	return (0);
reject:
	CTR4(KTR_CXGBE, "%s: stid %u, tid %u, REJECT (%d)", __func__, stid, tid,
	    reject_reason);

	if (e)
		t4_l2t_release(e);
	release_tid(sc, tid, lctx->ctrlq);

	if (__predict_true(m != NULL)) {
		m_adj(m, sizeof(*cpl));
		m->m_pkthdr.csum_flags |= (CSUM_IP_CHECKED | CSUM_IP_VALID |
		    CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
		m->m_pkthdr.csum_data = 0xffff;
		ifp->if_input(ifp, m);
	}

	return (reject_reason);
}

static void
synqe_to_protohdrs(struct synq_entry *synqe,
    const struct cpl_pass_establish *cpl, struct in_conninfo *inc,
    struct tcphdr *th, struct tcpopt *to)
{
	uint16_t tcp_opt = be16toh(cpl->tcp_opt);

	/* start off with the original SYN */
	pass_accept_req_to_protohdrs(synqe->syn, inc, th);

	/* modify parts to make it look like the ACK to our SYN|ACK */
	th->th_flags = TH_ACK;
	th->th_ack = synqe->iss + 1;
	th->th_seq = be32toh(cpl->rcv_isn);
	bzero(to, sizeof(*to));
	if (G_TCPOPT_TSTAMP(tcp_opt)) {
		to->to_flags |= TOF_TS;
		to->to_tsecr = synqe->ts;
	}
}

static int
do_pass_establish(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct port_info *pi;
	struct ifnet *ifp;
	const struct cpl_pass_establish *cpl = (const void *)(rss + 1);
#if defined(KTR) || defined(INVARIANTS)
	unsigned int stid = G_PASS_OPEN_TID(be32toh(cpl->tos_stid));
#endif
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(sc, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
	struct socket *so;
	struct tcphdr th;
	struct tcpopt to;
	struct in_conninfo inc;
	struct toepcb *toep;
	u_int txqid, rxqid;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PASS_ESTABLISH,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));
	KASSERT(synqe_flag(synqe, TPF_SYNQE),
	    ("%s: tid %u (ctx %p) not a synqe", __func__, tid, synqe));

	INP_INFO_WLOCK(&V_tcbinfo);	/* for syncache_expand */
	INP_WLOCK(inp);

	CTR6(KTR_CXGBE,
	    "%s: stid %u, tid %u, synqe %p (0x%x), inp_flags 0x%x",
	    __func__, stid, tid, synqe, synqe->flags, inp->inp_flags);

	if (__predict_false(inp->inp_flags & INP_DROPPED)) {
		/*
		 * The listening socket has closed.  The TOM must have aborted
		 * all the embryonic connections (including this one) that were
		 * on the lctx's synq.  do_abort_rpl for the tid is responsible
		 * for cleaning up.
		 */
		KASSERT(synqe_flag(synqe, TPF_ABORT_SHUTDOWN),
		    ("%s: listen socket dropped but tid %u not aborted.",
		    __func__, tid));

		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		return (0);
	}

	ifp = synqe->syn->m_pkthdr.rcvif;
	pi = ifp->if_softc;
	KASSERT(pi->adapter == sc,
	    ("%s: pi %p, sc %p mismatch", __func__, pi, sc));

	get_qids_from_mbuf(synqe->syn, &txqid, &rxqid);
	KASSERT(rxqid == iq_to_ofld_rxq(iq) - &sc->sge.ofld_rxq[0],
	    ("%s: CPL arrived on unexpected rxq.  %d %d", __func__, rxqid,
	    (int)(iq_to_ofld_rxq(iq) - &sc->sge.ofld_rxq[0])));

	toep = alloc_toepcb(pi, txqid, rxqid, M_NOWAIT);
	if (toep == NULL) {
reset:
		/* The reply to this abort will perform final cleanup */
		send_reset_synqe(TOEDEV(ifp), synqe);
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		return (0);
	}
	toep->tid = tid;
	toep->l2te = &sc->l2t->l2tab[synqe->l2e_idx];
	toep->ulp_mode = ULP_MODE_NONE;
	/* opt0 rcv_bufsiz initially, assumes its normal meaning later */
	toep->rx_credits = synqe->rcv_bufsize;

	so = inp->inp_socket;
	KASSERT(so != NULL, ("%s: socket is NULL", __func__));

	/* Come up with something that syncache_expand should be ok with. */
	synqe_to_protohdrs(synqe, cpl, &inc, &th, &to);

	/*
	 * No more need for anything in the mbuf that carried the
	 * CPL_PASS_ACCEPT_REQ.  Drop the CPL_PASS_ESTABLISH and toep pointer
	 * there.  XXX: bad form but I don't want to increase the size of synqe.
	 */
	m = synqe->syn;
	KASSERT(sizeof(*cpl) + sizeof(toep) <= m->m_len,
	    ("%s: no room in mbuf %p (m_len %d)", __func__, m, m->m_len));
	bcopy(cpl, mtod(m, void *), sizeof(*cpl));
	*(struct toepcb **)(mtod(m, struct cpl_pass_establish *) + 1) = toep;

	if (!toe_syncache_expand(&inc, &to, &th, &so) || so == NULL) {
		free_toepcb(toep);
		goto reset;
	}

	/* Done with the synqe */
	TAILQ_REMOVE(&lctx->synq, synqe, link);
	inp = release_lctx(sc, lctx);
	if (inp != NULL)
		INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	release_synqe(synqe);

	return (0);
}

void
t4_init_listen_cpl_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_PASS_OPEN_RPL, do_pass_open_rpl);
	t4_register_cpl_handler(sc, CPL_CLOSE_LISTSRV_RPL, do_close_server_rpl);
	t4_register_cpl_handler(sc, CPL_PASS_ACCEPT_REQ, do_pass_accept_req);
	t4_register_cpl_handler(sc, CPL_PASS_ESTABLISH, do_pass_establish);
}
#endif
