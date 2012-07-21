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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/toecore.h>

#ifdef TCP_OFFLOAD
#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

/* Module ops */
static int t4_tom_mod_load(void);
static int t4_tom_mod_unload(void);
static int t4_tom_modevent(module_t, int, void *);

/* ULD ops and helpers */
static int t4_tom_activate(struct adapter *);
static int t4_tom_deactivate(struct adapter *);

static struct uld_info tom_uld_info = {
	.uld_id = ULD_TOM,
	.activate = t4_tom_activate,
	.deactivate = t4_tom_deactivate,
};

static void queue_tid_release(struct adapter *, int);
static void release_offload_resources(struct toepcb *);
static int alloc_tid_tabs(struct tid_info *);
static void free_tid_tabs(struct tid_info *);
static void free_tom_data(struct adapter *, struct tom_data *);

struct toepcb *
alloc_toepcb(struct port_info *pi, int txqid, int rxqid, int flags)
{
	struct adapter *sc = pi->adapter;
	struct toepcb *toep;
	int tx_credits, txsd_total, len;

	/*
	 * The firmware counts tx work request credits in units of 16 bytes
	 * each.  Reserve room for an ABORT_REQ so the driver never has to worry
	 * about tx credits if it wants to abort a connection.
	 */
	tx_credits = sc->params.ofldq_wr_cred;
	tx_credits -= howmany(sizeof(struct cpl_abort_req), 16);

	/*
	 * Shortest possible tx work request is a fw_ofld_tx_data_wr + 1 byte
	 * immediate payload, and firmware counts tx work request credits in
	 * units of 16 byte.  Calculate the maximum work requests possible.
	 */
	txsd_total = tx_credits /
	    howmany((sizeof(struct fw_ofld_tx_data_wr) + 1), 16);

	if (txqid < 0)
		txqid = (arc4random() % pi->nofldtxq) + pi->first_ofld_txq;
	KASSERT(txqid >= pi->first_ofld_txq &&
	    txqid < pi->first_ofld_txq + pi->nofldtxq,
	    ("%s: txqid %d for port %p (first %d, n %d)", __func__, txqid, pi,
		pi->first_ofld_txq, pi->nofldtxq));

	if (rxqid < 0)
		rxqid = (arc4random() % pi->nofldrxq) + pi->first_ofld_rxq;
	KASSERT(rxqid >= pi->first_ofld_rxq &&
	    rxqid < pi->first_ofld_rxq + pi->nofldrxq,
	    ("%s: rxqid %d for port %p (first %d, n %d)", __func__, rxqid, pi,
		pi->first_ofld_rxq, pi->nofldrxq));

	len = offsetof(struct toepcb, txsd) +
	    txsd_total * sizeof(struct ofld_tx_sdesc);

	toep = malloc(len, M_CXGBE, M_ZERO | flags);
	if (toep == NULL)
		return (NULL);

	toep->td = sc->tom_softc;
	toep->port = pi;
	toep->tx_credits = tx_credits;
	toep->ofld_txq = &sc->sge.ofld_txq[txqid];
	toep->ofld_rxq = &sc->sge.ofld_rxq[rxqid];
	toep->ctrlq = &sc->sge.ctrlq[pi->port_id];
	toep->txsd_total = txsd_total;
	toep->txsd_avail = txsd_total;
	toep->txsd_pidx = 0;
	toep->txsd_cidx = 0;

	return (toep);
}

void
free_toepcb(struct toepcb *toep)
{

	KASSERT(toepcb_flag(toep, TPF_ATTACHED) == 0,
	    ("%s: attached to an inpcb", __func__));
	KASSERT(toepcb_flag(toep, TPF_CPL_PENDING) == 0,
	    ("%s: CPL pending", __func__));

	free(toep, M_CXGBE);
}

/*
 * Set up the socket for TCP offload.
 */
void
offload_socket(struct socket *so, struct toepcb *toep)
{
	struct tom_data *td = toep->td;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct sockbuf *sb;

	INP_WLOCK_ASSERT(inp);

	/* Update socket */
	sb = &so->so_snd;
	SOCKBUF_LOCK(sb);
	sb->sb_flags |= SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	sb->sb_flags |= SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);

	/* Update TCP PCB */
	tp->tod = &td->tod;
	tp->t_toe = toep;
	tp->t_flags |= TF_TOE;

	/* Install an extra hold on inp */
	toep->inp = inp;
	toepcb_set_flag(toep, TPF_ATTACHED);
	in_pcbref(inp);

	/* Add the TOE PCB to the active list */
	mtx_lock(&td->toep_list_lock);
	TAILQ_INSERT_HEAD(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);
}

/* This is _not_ the normal way to "unoffload" a socket. */
void
undo_offload_socket(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct toepcb *toep = tp->t_toe;
	struct tom_data *td = toep->td;
	struct sockbuf *sb;

	INP_WLOCK_ASSERT(inp);

	sb = &so->so_snd;
	SOCKBUF_LOCK(sb);
	sb->sb_flags &= ~SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	sb->sb_flags &= ~SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);

	tp->tod = NULL;
	tp->t_toe = NULL;
	tp->t_flags &= ~TF_TOE;

	toep->inp = NULL;
	toepcb_clr_flag(toep, TPF_ATTACHED);
	if (in_pcbrele_wlocked(inp))
		panic("%s: inp freed.", __func__);

	mtx_lock(&td->toep_list_lock);
	TAILQ_REMOVE(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);
}

static void
release_offload_resources(struct toepcb *toep)
{
	struct tom_data *td = toep->td;
	struct adapter *sc = td_adapter(td);
	int tid = toep->tid;

	KASSERT(toepcb_flag(toep, TPF_CPL_PENDING) == 0,
	    ("%s: %p has CPL pending.", __func__, toep));
	KASSERT(toepcb_flag(toep, TPF_ATTACHED) == 0,
	    ("%s: %p is still attached.", __func__, toep));

	CTR4(KTR_CXGBE, "%s: toep %p (tid %d, l2te %p)",
	    __func__, toep, tid, toep->l2te);

	if (toep->l2te)
		t4_l2t_release(toep->l2te);

	if (tid >= 0) {
		remove_tid(sc, tid);
		release_tid(sc, tid, toep->ctrlq);
	}

	mtx_lock(&td->toep_list_lock);
	TAILQ_REMOVE(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);

	free_toepcb(toep);
}

/*
 * The kernel is done with the TCP PCB and this is our opportunity to unhook the
 * toepcb hanging off of it.  If the TOE driver is also done with the toepcb (no
 * pending CPL) then it is time to release all resources tied to the toepcb.
 *
 * Also gets called when an offloaded active open fails and the TOM wants the
 * kernel to take the TCP PCB back.
 */
static void
t4_pcb_detach(struct toedev *tod __unused, struct tcpcb *tp)
{
#if defined(KTR) || defined(INVARIANTS)
	struct inpcb *inp = tp->t_inpcb;
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);

	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));
	KASSERT(toepcb_flag(toep, TPF_ATTACHED),
	    ("%s: not attached", __func__));

#ifdef KTR
	if (tp->t_state == TCPS_SYN_SENT) {
		CTR6(KTR_CXGBE, "%s: atid %d, toep %p (0x%x), inp %p (0x%x)",
		    __func__, toep->tid, toep, toep->flags, inp,
		    inp->inp_flags);
	} else {
		CTR6(KTR_CXGBE,
		    "t4_pcb_detach: tid %d (%s), toep %p (0x%x), inp %p (0x%x)",
		    toep->tid, tcpstates[tp->t_state], toep, toep->flags, inp,
		    inp->inp_flags);
	}
#endif

	tp->t_toe = NULL;
	tp->t_flags &= ~TF_TOE;
	toepcb_clr_flag(toep, TPF_ATTACHED);

	if (toepcb_flag(toep, TPF_CPL_PENDING) == 0)
		release_offload_resources(toep);
}

/*
 * The TOE driver will not receive any more CPLs for the tid associated with the
 * toepcb; release the hold on the inpcb.
 */
void
final_cpl_received(struct toepcb *toep)
{
	struct inpcb *inp = toep->inp;

	KASSERT(inp != NULL, ("%s: inp is NULL", __func__));
	INP_WLOCK_ASSERT(inp);
	KASSERT(toepcb_flag(toep, TPF_CPL_PENDING),
	    ("%s: CPL not pending already?", __func__));

	CTR6(KTR_CXGBE, "%s: tid %d, toep %p (0x%x), inp %p (0x%x)",
	    __func__, toep->tid, toep, toep->flags, inp, inp->inp_flags);

	toep->inp = NULL;
	toepcb_clr_flag(toep, TPF_CPL_PENDING);

	if (toepcb_flag(toep, TPF_ATTACHED) == 0)
		release_offload_resources(toep);

	if (!in_pcbrele_wlocked(inp))
		INP_WUNLOCK(inp);
}

void
insert_tid(struct adapter *sc, int tid, void *ctx)
{
	struct tid_info *t = &sc->tids;

	t->tid_tab[tid] = ctx;
	atomic_add_int(&t->tids_in_use, 1);
}

void *
lookup_tid(struct adapter *sc, int tid)
{
	struct tid_info *t = &sc->tids;

	return (t->tid_tab[tid]);
}

void
update_tid(struct adapter *sc, int tid, void *ctx)
{
	struct tid_info *t = &sc->tids;

	t->tid_tab[tid] = ctx;
}

void
remove_tid(struct adapter *sc, int tid)
{
	struct tid_info *t = &sc->tids;

	t->tid_tab[tid] = NULL;
	atomic_subtract_int(&t->tids_in_use, 1);
}

void
release_tid(struct adapter *sc, int tid, struct sge_wrq *ctrlq)
{
	struct wrqe *wr;
	struct cpl_tid_release *req;

	wr = alloc_wrqe(sizeof(*req), ctrlq);
	if (wr == NULL) {
		queue_tid_release(sc, tid);	/* defer */
		return;
	}
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_TID_RELEASE, tid);

	t4_wrq_tx(sc, wr);
}

static void
queue_tid_release(struct adapter *sc, int tid)
{

	CXGBE_UNIMPLEMENTED("deferred tid release");
}

/*
 * What mtu_idx to use, given a 4-tuple and/or an MSS cap
 */
int
find_best_mtu_idx(struct adapter *sc, struct in_conninfo *inc, int pmss)
{
	unsigned short *mtus = &sc->params.mtus[0];
	int i = 0, mss;

	KASSERT(inc != NULL || pmss > 0,
	    ("%s: at least one of inc/pmss must be specified", __func__));

	mss = inc ? tcp_mssopt(inc) : pmss;
	if (pmss > 0 && mss > pmss)
		mss = pmss;

	while (i < NMTUS - 1 && mtus[i + 1] <= mss + 40)
		++i;

	return (i);
}

/*
 * Determine the receive window size for a socket.
 */
u_long
select_rcv_wnd(struct socket *so)
{
	unsigned long wnd;

	SOCKBUF_LOCK_ASSERT(&so->so_rcv);

	wnd = sbspace(&so->so_rcv);
	if (wnd < MIN_RCV_WND)
		wnd = MIN_RCV_WND;

	return min(wnd, MAX_RCV_WND);
}

int
select_rcv_wscale(void)
{
	int wscale = 0;
	unsigned long space = sb_max;

	if (space > MAX_RCV_WND)
		space = MAX_RCV_WND;

	while (wscale < TCP_MAX_WINSHIFT && (TCP_MAXWIN << wscale) < space)
		wscale++;

	return (wscale);
}

extern int always_keepalive;
#define VIID_SMACIDX(v)	(((unsigned int)(v) & 0x7f) << 1)

/*
 * socket so could be a listening socket too.
 */
uint64_t
calc_opt0(struct socket *so, struct port_info *pi, struct l2t_entry *e,
    int mtu_idx, int rscale, int rx_credits, int ulp_mode)
{
	uint64_t opt0;

	KASSERT(rx_credits <= M_RCV_BUFSIZ,
	    ("%s: rcv_bufsiz too high", __func__));

	opt0 = F_TCAM_BYPASS | V_WND_SCALE(rscale) | V_MSS_IDX(mtu_idx) |
	    V_ULP_MODE(ulp_mode) | V_RCV_BUFSIZ(rx_credits);

	if (so != NULL) {
		struct inpcb *inp = sotoinpcb(so);
		struct tcpcb *tp = intotcpcb(inp);
		int keepalive = always_keepalive ||
		    so_options_get(so) & SO_KEEPALIVE;

		opt0 |= V_NAGLE((tp->t_flags & TF_NODELAY) == 0);
		opt0 |= V_KEEP_ALIVE(keepalive != 0);
	}

	if (e != NULL)
		opt0 |= V_L2T_IDX(e->idx);

	if (pi != NULL) {
		opt0 |= V_SMAC_SEL(VIID_SMACIDX(pi->viid));
		opt0 |= V_TX_CHAN(pi->tx_chan);
	}

	return htobe64(opt0);
}

#define FILTER_SEL_WIDTH_P_FC (3 + 1)
#define FILTER_SEL_WIDTH_VIN_P_FC (6 + 7 + FILTER_SEL_WIDTH_P_FC)
#define FILTER_SEL_WIDTH_TAG_P_FC (3 + FILTER_SEL_WIDTH_VIN_P_FC)
#define FILTER_SEL_WIDTH_VLD_TAG_P_FC (1 + FILTER_SEL_WIDTH_TAG_P_FC)
#define VLAN_NONE 0xfff
#define FILTER_SEL_VLAN_NONE 0xffff

uint32_t
select_ntuple(struct port_info *pi, struct l2t_entry *e, uint32_t filter_mode)
{
	uint16_t viid = pi->viid;
	uint32_t ntuple = 0;

	if (filter_mode == HW_TPL_FR_MT_PR_IV_P_FC) {
                if (e->vlan == VLAN_NONE)
			ntuple |= FILTER_SEL_VLAN_NONE << FILTER_SEL_WIDTH_P_FC;
                else {
                        ntuple |= e->vlan << FILTER_SEL_WIDTH_P_FC;
                        ntuple |= 1 << FILTER_SEL_WIDTH_VLD_TAG_P_FC;
                }
                ntuple |= e->lport << S_PORT;
		ntuple |= IPPROTO_TCP << FILTER_SEL_WIDTH_VLD_TAG_P_FC;
	} else if (filter_mode == HW_TPL_FR_MT_PR_OV_P_FC) {
                ntuple |= G_FW_VIID_VIN(viid) << FILTER_SEL_WIDTH_P_FC;
                ntuple |= G_FW_VIID_PFN(viid) << FILTER_SEL_WIDTH_VIN_P_FC;
                ntuple |= G_FW_VIID_VIVLD(viid) << FILTER_SEL_WIDTH_TAG_P_FC;
                ntuple |= e->lport << S_PORT;
		ntuple |= IPPROTO_TCP << FILTER_SEL_WIDTH_VLD_TAG_P_FC;
        }

	return (htobe32(ntuple));
}

static int
alloc_tid_tabs(struct tid_info *t)
{
	size_t size;
	unsigned int i;

	size = t->ntids * sizeof(*t->tid_tab) +
	    t->natids * sizeof(*t->atid_tab) +
	    t->nstids * sizeof(*t->stid_tab);

	t->tid_tab = malloc(size, M_CXGBE, M_ZERO | M_NOWAIT);
	if (t->tid_tab == NULL)
		return (ENOMEM);

	mtx_init(&t->atid_lock, "atid lock", NULL, MTX_DEF);
	t->atid_tab = (union aopen_entry *)&t->tid_tab[t->ntids];
	t->afree = t->atid_tab;
	t->atids_in_use = 0;
	for (i = 1; i < t->natids; i++)
		t->atid_tab[i - 1].next = &t->atid_tab[i];
	t->atid_tab[t->natids - 1].next = NULL;

	mtx_init(&t->stid_lock, "stid lock", NULL, MTX_DEF);
	t->stid_tab = (union serv_entry *)&t->atid_tab[t->natids];
	t->sfree = t->stid_tab;
	t->stids_in_use = 0;
	for (i = 1; i < t->nstids; i++)
		t->stid_tab[i - 1].next = &t->stid_tab[i];
	t->stid_tab[t->nstids - 1].next = NULL;

	atomic_store_rel_int(&t->tids_in_use, 0);

	return (0);
}

static void
free_tid_tabs(struct tid_info *t)
{
	KASSERT(t->tids_in_use == 0,
	    ("%s: %d tids still in use.", __func__, t->tids_in_use));
	KASSERT(t->atids_in_use == 0,
	    ("%s: %d atids still in use.", __func__, t->atids_in_use));
	KASSERT(t->stids_in_use == 0,
	    ("%s: %d tids still in use.", __func__, t->stids_in_use));

	free(t->tid_tab, M_CXGBE);
	t->tid_tab = NULL;

	if (mtx_initialized(&t->atid_lock))
		mtx_destroy(&t->atid_lock);
	if (mtx_initialized(&t->stid_lock))
		mtx_destroy(&t->stid_lock);
}

static void
free_tom_data(struct adapter *sc, struct tom_data *td)
{
	KASSERT(TAILQ_EMPTY(&td->toep_list),
	    ("%s: TOE PCB list is not empty.", __func__));
	KASSERT(td->lctx_count == 0,
	    ("%s: lctx hash table is not empty.", __func__));

	t4_uninit_l2t_cpl_handlers(sc);

	if (td->listen_mask != 0)
		hashdestroy(td->listen_hash, M_CXGBE, td->listen_mask);

	if (mtx_initialized(&td->lctx_hash_lock))
		mtx_destroy(&td->lctx_hash_lock);
	if (mtx_initialized(&td->toep_list_lock))
		mtx_destroy(&td->toep_list_lock);

	free_tid_tabs(&sc->tids);
	free(td, M_CXGBE);
}

/*
 * Ground control to Major TOM
 * Commencing countdown, engines on
 */
static int
t4_tom_activate(struct adapter *sc)
{
	struct tom_data *td;
	struct toedev *tod;
	int i, rc;

	ADAPTER_LOCK_ASSERT_OWNED(sc);	/* for sc->flags */

	/* per-adapter softc for TOM */
	td = malloc(sizeof(*td), M_CXGBE, M_ZERO | M_NOWAIT);
	if (td == NULL)
		return (ENOMEM);

	/* List of TOE PCBs and associated lock */
	mtx_init(&td->toep_list_lock, "PCB list lock", NULL, MTX_DEF);
	TAILQ_INIT(&td->toep_list);

	/* Listen context */
	mtx_init(&td->lctx_hash_lock, "lctx hash lock", NULL, MTX_DEF);
	td->listen_hash = hashinit_flags(LISTEN_HASH_SIZE, M_CXGBE,
	    &td->listen_mask, HASH_NOWAIT);

	/* TID tables */
	rc = alloc_tid_tabs(&sc->tids);
	if (rc != 0)
		goto done;

	/* CPL handlers */
	t4_init_connect_cpl_handlers(sc);
	t4_init_l2t_cpl_handlers(sc);
	t4_init_listen_cpl_handlers(sc);
	t4_init_cpl_io_handlers(sc);

	/* toedev ops */
	tod = &td->tod;
	init_toedev(tod);
	tod->tod_softc = sc;
	tod->tod_connect = t4_connect;
	tod->tod_listen_start = t4_listen_start;
	tod->tod_listen_stop = t4_listen_stop;
	tod->tod_rcvd = t4_rcvd;
	tod->tod_output = t4_tod_output;
	tod->tod_send_rst = t4_send_rst;
	tod->tod_send_fin = t4_send_fin;
	tod->tod_pcb_detach = t4_pcb_detach;
	tod->tod_l2_update = t4_l2_update;
	tod->tod_syncache_added = t4_syncache_added;
	tod->tod_syncache_removed = t4_syncache_removed;
	tod->tod_syncache_respond = t4_syncache_respond;
	tod->tod_offload_socket = t4_offload_socket;

	for_each_port(sc, i)
		TOEDEV(sc->port[i]->ifp) = &td->tod;

	sc->tom_softc = td;
	sc->flags |= TOM_INIT_DONE;
	register_toedev(sc->tom_softc);

done:
	if (rc != 0)
		free_tom_data(sc, td);
	return (rc);
}

static int
t4_tom_deactivate(struct adapter *sc)
{
	int rc = 0;
	struct tom_data *td = sc->tom_softc;

	ADAPTER_LOCK_ASSERT_OWNED(sc);	/* for sc->flags */

	if (td == NULL)
		return (0);	/* XXX. KASSERT? */

	if (sc->offload_map != 0)
		return (EBUSY);	/* at least one port has IFCAP_TOE enabled */

	mtx_lock(&td->toep_list_lock);
	if (!TAILQ_EMPTY(&td->toep_list))
		rc = EBUSY;
	mtx_unlock(&td->toep_list_lock);

	mtx_lock(&td->lctx_hash_lock);
	if (td->lctx_count > 0)
		rc = EBUSY;
	mtx_unlock(&td->lctx_hash_lock);

	if (rc == 0) {
		unregister_toedev(sc->tom_softc);
		free_tom_data(sc, td);
		sc->tom_softc = NULL;
		sc->flags &= ~TOM_INIT_DONE;
	}

	return (rc);
}

static int
t4_tom_mod_load(void)
{
	int rc;

	rc = t4_register_uld(&tom_uld_info);
	if (rc != 0)
		t4_tom_mod_unload();

	return (rc);
}

static void
tom_uninit(struct adapter *sc, void *arg __unused)
{
	/* Try to free resources (works only if no port has IFCAP_TOE) */
	ADAPTER_LOCK(sc);
	if (sc->flags & TOM_INIT_DONE)
		t4_deactivate_uld(sc, ULD_TOM);
	ADAPTER_UNLOCK(sc);
}

static int
t4_tom_mod_unload(void)
{
	t4_iterate(tom_uninit, NULL);

	if (t4_unregister_uld(&tom_uld_info) == EBUSY)
		return (EBUSY);

	return (0);
}
#endif	/* TCP_OFFLOAD */

static int
t4_tom_modevent(module_t mod, int cmd, void *arg)
{
	int rc = 0;

#ifdef TCP_OFFLOAD
	switch (cmd) {
	case MOD_LOAD:
		rc = t4_tom_mod_load();
		break;

	case MOD_UNLOAD:
		rc = t4_tom_mod_unload();
		break;

	default:
		rc = EINVAL;
	}
#else
	printf("t4_tom: compiled without TCP_OFFLOAD support.\n");
	rc = EOPNOTSUPP;
#endif
	return (rc);
}

static moduledata_t t4_tom_moddata= {
	"t4_tom",
	t4_tom_modevent,
	0
};

MODULE_VERSION(t4_tom, 1);
MODULE_DEPEND(t4_tom, toecore, 1, 1, 1);
MODULE_DEPEND(t4_tom, t4nex, 1, 1, 1);
DECLARE_MODULE(t4_tom, t4_tom_moddata, SI_SUB_EXEC, SI_ORDER_ANY);
