/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2015 Chelsio Communications, Inc.
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 *
 */

/*
 * cxgbei implementation of iSCSI Common Layer kobj(9) interface.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/uio.h>
#include <machine/bus.h>
#include <vm/uma.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>

#include <dev/iscsi/icl.h>
#include <dev/iscsi/iscsi_proto.h>
#include <icl_conn_if.h>

#include "common/common.h"
#include "common/t4_tcb.h"
#include "tom/t4_tom.h"
#include "cxgbei.h"

SYSCTL_NODE(_kern_icl, OID_AUTO, cxgbei, CTLFLAG_RD, 0, "Chelsio iSCSI offload");
static int coalesce = 1;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, coalesce, CTLFLAG_RWTUN,
	&coalesce, 0, "Try to coalesce PDUs before sending");
static int partial_receive_len = 128 * 1024;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, partial_receive_len, CTLFLAG_RWTUN,
    &partial_receive_len, 0, "Minimum read size for partially received "
    "data segment");
static int sendspace = 1048576;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, sendspace, CTLFLAG_RWTUN,
    &sendspace, 0, "Default send socket buffer size");
static int recvspace = 1048576;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, recvspace, CTLFLAG_RWTUN,
    &recvspace, 0, "Default receive socket buffer size");

static uma_zone_t icl_transfer_zone;

static volatile u_int icl_cxgbei_ncons;

#define ICL_CONN_LOCK(X)		mtx_lock(X->ic_lock)
#define ICL_CONN_UNLOCK(X)		mtx_unlock(X->ic_lock)
#define ICL_CONN_LOCK_ASSERT(X)		mtx_assert(X->ic_lock, MA_OWNED)
#define ICL_CONN_LOCK_ASSERT_NOT(X)	mtx_assert(X->ic_lock, MA_NOTOWNED)

struct icl_pdu *icl_cxgbei_new_pdu(int);
void icl_cxgbei_new_pdu_set_conn(struct icl_pdu *, struct icl_conn *);

static icl_conn_new_pdu_t	icl_cxgbei_conn_new_pdu;
icl_conn_pdu_free_t	icl_cxgbei_conn_pdu_free;
static icl_conn_pdu_data_segment_length_t
				    icl_cxgbei_conn_pdu_data_segment_length;
static icl_conn_pdu_append_data_t	icl_cxgbei_conn_pdu_append_data;
static icl_conn_pdu_get_data_t	icl_cxgbei_conn_pdu_get_data;
static icl_conn_pdu_queue_t	icl_cxgbei_conn_pdu_queue;
static icl_conn_handoff_t	icl_cxgbei_conn_handoff;
static icl_conn_free_t		icl_cxgbei_conn_free;
static icl_conn_close_t		icl_cxgbei_conn_close;
static icl_conn_task_setup_t	icl_cxgbei_conn_task_setup;
static icl_conn_task_done_t	icl_cxgbei_conn_task_done;
static icl_conn_transfer_setup_t	icl_cxgbei_conn_transfer_setup;
static icl_conn_transfer_done_t	icl_cxgbei_conn_transfer_done;

static kobj_method_t icl_cxgbei_methods[] = {
	KOBJMETHOD(icl_conn_new_pdu, icl_cxgbei_conn_new_pdu),
	KOBJMETHOD(icl_conn_pdu_free, icl_cxgbei_conn_pdu_free),
	KOBJMETHOD(icl_conn_pdu_data_segment_length,
	    icl_cxgbei_conn_pdu_data_segment_length),
	KOBJMETHOD(icl_conn_pdu_append_data, icl_cxgbei_conn_pdu_append_data),
	KOBJMETHOD(icl_conn_pdu_get_data, icl_cxgbei_conn_pdu_get_data),
	KOBJMETHOD(icl_conn_pdu_queue, icl_cxgbei_conn_pdu_queue),
	KOBJMETHOD(icl_conn_handoff, icl_cxgbei_conn_handoff),
	KOBJMETHOD(icl_conn_free, icl_cxgbei_conn_free),
	KOBJMETHOD(icl_conn_close, icl_cxgbei_conn_close),
	KOBJMETHOD(icl_conn_task_setup, icl_cxgbei_conn_task_setup),
	KOBJMETHOD(icl_conn_task_done, icl_cxgbei_conn_task_done),
	KOBJMETHOD(icl_conn_transfer_setup, icl_cxgbei_conn_transfer_setup),
	KOBJMETHOD(icl_conn_transfer_done, icl_cxgbei_conn_transfer_done),
	{ 0, 0 }
};

DEFINE_CLASS(icl_cxgbei, icl_cxgbei_methods, sizeof(struct icl_cxgbei_conn));

#if 0
/*
 * Subtract another 256 for AHS from MAX_DSL if AHS could be used.
 */
#define CXGBEI_MAX_PDU 16224
#define CXGBEI_MAX_DSL (CXGBEI_MAX_PDU - sizeof(struct iscsi_bhs) - 8)
#endif
#define CXGBEI_MAX_DSL 8192
#define CXGBEI_MAX_PDU (CXGBEI_MAX_DSL + sizeof(struct iscsi_bhs) + 8)

void
icl_cxgbei_conn_pdu_free(struct icl_conn *ic, struct icl_pdu *ip)
{
#ifdef INVARIANTS
	struct icl_cxgbei_pdu *icp = ip_to_icp(ip);
#endif

	MPASS(icp->icp_signature == CXGBEI_PDU_SIGNATURE);
	MPASS(ic == ip->ip_conn);
	MPASS(ip->ip_bhs_mbuf != NULL);

	m_freem(ip->ip_ahs_mbuf);
	m_freem(ip->ip_data_mbuf);
	m_freem(ip->ip_bhs_mbuf);	/* storage for icl_cxgbei_pdu itself */

#ifdef DIAGNOSTIC
	if (__predict_true(ic != NULL))
		refcount_release(&ic->ic_outstanding_pdus);
#endif
}

struct icl_pdu *
icl_cxgbei_new_pdu(int flags)
{
	struct icl_cxgbei_pdu *icp;
	struct icl_pdu *ip;
	struct mbuf *m;
	uintptr_t a;

	m = m_gethdr(flags, MT_DATA);
	if (__predict_false(m == NULL))
		return (NULL);

	a = roundup2(mtod(m, uintptr_t), _Alignof(struct icl_cxgbei_pdu));
	icp = (struct icl_cxgbei_pdu *)a;
	bzero(icp, sizeof(*icp));

	icp->icp_signature = CXGBEI_PDU_SIGNATURE;
	ip = &icp->ip;
	ip->ip_bhs_mbuf = m;

	a = roundup2((uintptr_t)(icp + 1), _Alignof(struct iscsi_bhs *));
	ip->ip_bhs = (struct iscsi_bhs *)a;
#ifdef INVARIANTS
	/* Everything must fit entirely in the mbuf. */
	a = (uintptr_t)(ip->ip_bhs + 1);
	MPASS(a <= (uintptr_t)m + MSIZE);
#endif
	bzero(ip->ip_bhs, sizeof(*ip->ip_bhs));

	m->m_data = (void *)ip->ip_bhs;
	m->m_len = sizeof(struct iscsi_bhs);
	m->m_pkthdr.len = m->m_len;

	return (ip);
}

void
icl_cxgbei_new_pdu_set_conn(struct icl_pdu *ip, struct icl_conn *ic)
{

	ip->ip_conn = ic;
#ifdef DIAGNOSTIC
	refcount_acquire(&ic->ic_outstanding_pdus);
#endif
}

/*
 * Allocate icl_pdu with empty BHS to fill up by the caller.
 */
static struct icl_pdu *
icl_cxgbei_conn_new_pdu(struct icl_conn *ic, int flags)
{
	struct icl_pdu *ip;

	ip = icl_cxgbei_new_pdu(flags);
	if (__predict_false(ip == NULL))
		return (NULL);
	icl_cxgbei_new_pdu_set_conn(ip, ic);

	return (ip);
}

static size_t
icl_pdu_data_segment_length(const struct icl_pdu *request)
{
	uint32_t len = 0;

	len += request->ip_bhs->bhs_data_segment_len[0];
	len <<= 8;
	len += request->ip_bhs->bhs_data_segment_len[1];
	len <<= 8;
	len += request->ip_bhs->bhs_data_segment_len[2];

	return (len);
}

size_t
icl_cxgbei_conn_pdu_data_segment_length(struct icl_conn *ic,
    const struct icl_pdu *request)
{

	return (icl_pdu_data_segment_length(request));
}

static uint32_t
icl_conn_build_tasktag(struct icl_conn *ic, uint32_t tag)
{
	return tag;
}

static struct mbuf *
finalize_pdu(struct icl_cxgbei_conn *icc, struct icl_cxgbei_pdu *icp)
{
	struct icl_pdu *ip = &icp->ip;
	uint8_t ulp_submode, padding;
	struct mbuf *m, *last;
	struct iscsi_bhs *bhs;

	/*
	 * Fix up the data segment mbuf first.
	 */
	m = ip->ip_data_mbuf;
	ulp_submode = icc->ulp_submode;
	if (m) {
		last = m_last(m);

		/*
		 * Round up the data segment to a 4B boundary.  Pad with 0 if
		 * necessary.  There will definitely be room in the mbuf.
		 */
		padding = roundup2(ip->ip_data_len, 4) - ip->ip_data_len;
		if (padding) {
			bzero(mtod(last, uint8_t *) + last->m_len, padding);
			last->m_len += padding;
		}
	} else {
		MPASS(ip->ip_data_len == 0);
		ulp_submode &= ~ULP_CRC_DATA;
		padding = 0;
	}

	/*
	 * Now the header mbuf that has the BHS.
	 */
	m = ip->ip_bhs_mbuf;
	MPASS(m->m_pkthdr.len == sizeof(struct iscsi_bhs));
	MPASS(m->m_len == sizeof(struct iscsi_bhs));

	bhs = ip->ip_bhs;
	bhs->bhs_data_segment_len[2] = ip->ip_data_len;
	bhs->bhs_data_segment_len[1] = ip->ip_data_len >> 8;
	bhs->bhs_data_segment_len[0] = ip->ip_data_len >> 16;

	/* "Convert" PDU to mbuf chain.  Do not use icp/ip after this. */
	m->m_pkthdr.len = sizeof(struct iscsi_bhs) + ip->ip_data_len + padding;
	m->m_next = ip->ip_data_mbuf;
	set_mbuf_ulp_submode(m, ulp_submode);
#ifdef INVARIANTS
	bzero(icp, sizeof(*icp));
#endif
#ifdef DIAGNOSTIC
	refcount_release(&icc->ic.ic_outstanding_pdus);
#endif

	return (m);
}

int
icl_cxgbei_conn_pdu_append_data(struct icl_conn *ic, struct icl_pdu *ip,
    const void *addr, size_t len, int flags)
{
	struct mbuf *m;
#ifdef INVARIANTS
	struct icl_cxgbei_pdu *icp = ip_to_icp(ip);
#endif

	MPASS(icp->icp_signature == CXGBEI_PDU_SIGNATURE);
	MPASS(ic == ip->ip_conn);
	KASSERT(len > 0, ("%s: len is %jd", __func__, (intmax_t)len));

	m = ip->ip_data_mbuf;
	if (m == NULL) {
		m = m_getjcl(M_NOWAIT, MT_DATA, 0, MJUM16BYTES);
		if (__predict_false(m == NULL))
			return (ENOMEM);

		ip->ip_data_mbuf = m;
	}

	if (__predict_true(m_append(m, len, addr) != 0)) {
		ip->ip_data_len += len;
		MPASS(ip->ip_data_len <= CXGBEI_MAX_DSL);
		return (0);
	} else {
	    	if (flags & M_WAITOK) {
			CXGBE_UNIMPLEMENTED("fail safe append");
		}
		ip->ip_data_len = m_length(m, NULL);
		return (1);
	}
}

void
icl_cxgbei_conn_pdu_get_data(struct icl_conn *ic, struct icl_pdu *ip,
    size_t off, void *addr, size_t len)
{
	struct icl_cxgbei_pdu *icp = ip_to_icp(ip);

	if (icp->pdu_flags & SBUF_ULP_FLAG_DATA_DDPED)
		return; /* data is DDP'ed, no need to copy */
	m_copydata(ip->ip_data_mbuf, off, len, addr);
}

void
icl_cxgbei_conn_pdu_queue(struct icl_conn *ic, struct icl_pdu *ip)
{
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);
	struct icl_cxgbei_pdu *icp = ip_to_icp(ip);
	struct socket *so = ic->ic_socket;
	struct toepcb *toep = icc->toep;
	struct inpcb *inp;
	struct mbuf *m;

	MPASS(ic == ip->ip_conn);
	MPASS(ip->ip_bhs_mbuf != NULL);
	/* The kernel doesn't generate PDUs with AHS. */
	MPASS(ip->ip_ahs_mbuf == NULL && ip->ip_ahs_len == 0);

	ICL_CONN_LOCK_ASSERT(ic);
	/* NOTE: sowriteable without so_snd lock is a mostly harmless race. */
	if (ic->ic_disconnecting || so == NULL || !sowriteable(so)) {
		icl_cxgbei_conn_pdu_free(ic, ip);
		return;
	}

	m = finalize_pdu(icc, icp);
	M_ASSERTPKTHDR(m);
	MPASS((m->m_pkthdr.len & 3) == 0);
	MPASS(m->m_pkthdr.len + 8 <= CXGBEI_MAX_PDU);

	/*
	 * Do not get inp from toep->inp as the toepcb might have detached
	 * already.
	 */
	inp = sotoinpcb(so);
	INP_WLOCK(inp);
	if (__predict_false(inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) ||
	    __predict_false((toep->flags & TPF_ATTACHED) == 0))
		m_freem(m);
	else {
		mbufq_enqueue(&toep->ulp_pduq, m);
		t4_push_pdus(icc->sc, toep, 0);
	}
	INP_WUNLOCK(inp);
}

static struct icl_conn *
icl_cxgbei_new_conn(const char *name, struct mtx *lock)
{
	struct icl_cxgbei_conn *icc;
	struct icl_conn *ic;

	refcount_acquire(&icl_cxgbei_ncons);

	icc = (struct icl_cxgbei_conn *)kobj_create(&icl_cxgbei_class, M_CXGBE,
	    M_WAITOK | M_ZERO);
	icc->icc_signature = CXGBEI_CONN_SIGNATURE;
	STAILQ_INIT(&icc->rcvd_pdus);

	ic = &icc->ic;
	ic->ic_lock = lock;

	/* XXXNP: review.  Most of these icl_conn fields aren't really used */
	STAILQ_INIT(&ic->ic_to_send);
	cv_init(&ic->ic_send_cv, "icl_cxgbei_tx");
	cv_init(&ic->ic_receive_cv, "icl_cxgbei_rx");
#ifdef DIAGNOSTIC
	refcount_init(&ic->ic_outstanding_pdus, 0);
#endif
	ic->ic_max_data_segment_length = CXGBEI_MAX_DSL;
	ic->ic_name = name;
	ic->ic_offload = "cxgbei";
	ic->ic_unmapped = false;

	CTR2(KTR_CXGBE, "%s: icc %p", __func__, icc);

	return (ic);
}

void
icl_cxgbei_conn_free(struct icl_conn *ic)
{
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);

	CTR2(KTR_CXGBE, "%s: icc %p", __func__, icc);

	cv_destroy(&ic->ic_send_cv);
	cv_destroy(&ic->ic_receive_cv);

	kobj_delete((struct kobj *)icc, M_CXGBE);
	refcount_release(&icl_cxgbei_ncons);
}

static int
icl_cxgbei_setsockopt(struct icl_conn *ic, struct socket *so)
{
	size_t minspace;
	struct sockopt opt;
	int error, one = 1;

	/*
	 * For sendspace, this is required because the current code cannot
	 * send a PDU in pieces; thus, the minimum buffer size is equal
	 * to the maximum PDU size.  "+4" is to account for possible padding.
	 *
	 * What we should actually do here is to use autoscaling, but set
	 * some minimal buffer size to "minspace".  I don't know a way to do
	 * that, though.
	 */
	minspace = sizeof(struct iscsi_bhs) + ic->ic_max_data_segment_length +
	    ISCSI_HEADER_DIGEST_SIZE + ISCSI_DATA_DIGEST_SIZE + 4;
	if (sendspace < minspace)
		sendspace = minspace;
	if (recvspace < minspace)
		recvspace = minspace;

	error = soreserve(so, sendspace, recvspace);
	if (error != 0) {
		icl_cxgbei_conn_close(ic);
		return (error);
	}
	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_flags |= SB_AUTOSIZE;
	SOCKBUF_UNLOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);
	so->so_rcv.sb_flags |= SB_AUTOSIZE;
	SOCKBUF_UNLOCK(&so->so_rcv);

	/*
	 * Disable Nagle.
	 */
	bzero(&opt, sizeof(opt));
	opt.sopt_dir = SOPT_SET;
	opt.sopt_level = IPPROTO_TCP;
	opt.sopt_name = TCP_NODELAY;
	opt.sopt_val = &one;
	opt.sopt_valsize = sizeof(one);
	error = sosetopt(so, &opt);
	if (error != 0) {
		icl_cxgbei_conn_close(ic);
		return (error);
	}

	return (0);
}

/*
 * Request/response structure used to find out the adapter offloading a socket.
 */
struct find_ofld_adapter_rr {
	struct socket *so;
	struct adapter *sc;	/* result */
};

static void
find_offload_adapter(struct adapter *sc, void *arg)
{
	struct find_ofld_adapter_rr *fa = arg;
	struct socket *so = fa->so;
	struct tom_data *td = sc->tom_softc;
	struct tcpcb *tp;
	struct inpcb *inp;

	/* Non-TCP were filtered out earlier. */
	MPASS(so->so_proto->pr_protocol == IPPROTO_TCP);

	if (fa->sc != NULL)
		return;	/* Found already. */

	if (td == NULL)
		return;	/* TOE not enabled on this adapter. */

	inp = sotoinpcb(so);
	INP_WLOCK(inp);
	if ((inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) == 0) {
		tp = intotcpcb(inp);
		if (tp->t_flags & TF_TOE && tp->tod == &td->tod)
			fa->sc = sc;	/* Found. */
	}
	INP_WUNLOCK(inp);
}

/* XXXNP: move this to t4_tom. */
static void
send_iscsi_flowc_wr(struct adapter *sc, struct toepcb *toep, int maxlen)
{
	struct wrqe *wr;
	struct fw_flowc_wr *flowc;
	const u_int nparams = 1;
	u_int flowclen;
	struct ofld_tx_sdesc *txsd = &toep->txsd[toep->txsd_pidx];

	flowclen = sizeof(*flowc) + nparams * sizeof(struct fw_flowc_mnemval);

	wr = alloc_wrqe(roundup2(flowclen, 16), toep->ofld_txq);
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

	flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_TXDATAPLEN_MAX;
	flowc->mnemval[0].val = htobe32(maxlen);

	txsd->tx_credits = howmany(flowclen, 16);
	txsd->plen = 0;
	KASSERT(toep->tx_credits >= txsd->tx_credits && toep->txsd_avail > 0,
	    ("%s: not enough credits (%d)", __func__, toep->tx_credits));
	toep->tx_credits -= txsd->tx_credits;
	if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
		toep->txsd_pidx = 0;
	toep->txsd_avail--;

        t4_wrq_tx(sc, wr);
}

static void
set_ulp_mode_iscsi(struct adapter *sc, struct toepcb *toep, int hcrc, int dcrc)
{
	uint64_t val = ULP_MODE_ISCSI;

	if (hcrc)
		val |= ULP_CRC_HEADER << 4;
	if (dcrc)
		val |= ULP_CRC_DATA << 4;

	CTR4(KTR_CXGBE, "%s: tid %u, ULP_MODE_ISCSI, CRC hdr=%d data=%d",
	    __func__, toep->tid, hcrc, dcrc);

	t4_set_tcb_field(sc, toep->ctrlq, toep, W_TCB_ULP_TYPE,
	    V_TCB_ULP_TYPE(M_TCB_ULP_TYPE) | V_TCB_ULP_RAW(M_TCB_ULP_RAW), val,
	    0, 0);
}

/*
 * XXXNP: Who is responsible for cleaning up the socket if this returns with an
 * error?  Review all error paths.
 *
 * XXXNP: What happens to the socket's fd reference if the operation is
 * successful, and how does that affect the socket's life cycle?
 */
int
icl_cxgbei_conn_handoff(struct icl_conn *ic, int fd)
{
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);
	struct find_ofld_adapter_rr fa;
	struct file *fp;
	struct socket *so;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct toepcb *toep;
	cap_rights_t rights;
	int error;

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);
	ICL_CONN_LOCK_ASSERT_NOT(ic);

	/*
	 * Steal the socket from userland.
	 */
	error = fget(curthread, fd,
	    cap_rights_init(&rights, CAP_SOCK_CLIENT), &fp);
	if (error != 0)
		return (error);
	if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, curthread);
		return (EINVAL);
	}
	so = fp->f_data;
	if (so->so_type != SOCK_STREAM ||
	    so->so_proto->pr_protocol != IPPROTO_TCP) {
		fdrop(fp, curthread);
		return (EINVAL);
	}

	ICL_CONN_LOCK(ic);
	if (ic->ic_socket != NULL) {
		ICL_CONN_UNLOCK(ic);
		fdrop(fp, curthread);
		return (EBUSY);
	}
	ic->ic_disconnecting = false;
	ic->ic_socket = so;
	fp->f_ops = &badfileops;
	fp->f_data = NULL;
	fdrop(fp, curthread);
	ICL_CONN_UNLOCK(ic);

	/* Find the adapter offloading this socket. */
	fa.sc = NULL;
	fa.so = so;
	t4_iterate(find_offload_adapter, &fa);
	if (fa.sc == NULL)
		return (EINVAL);
	icc->sc = fa.sc;

	error = icl_cxgbei_setsockopt(ic, so);
	if (error)
		return (error);

	inp = sotoinpcb(so);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);
	if (inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT))
		error = EBUSY;
	else {
		/*
		 * socket could not have been "unoffloaded" if here.
		 */
		MPASS(tp->t_flags & TF_TOE);
		MPASS(tp->tod != NULL);
		MPASS(tp->t_toe != NULL);
		toep = tp->t_toe;
		MPASS(toep->vi->pi->adapter == icc->sc);
		icc->toep = toep;
		icc->cwt = cxgbei_select_worker_thread(icc);
		icc->ulp_submode = 0;
		if (ic->ic_header_crc32c)
			icc->ulp_submode |= ULP_CRC_HEADER;
		if (ic->ic_data_crc32c)
			icc->ulp_submode |= ULP_CRC_DATA;
		so->so_options |= SO_NO_DDP;
		toep->ulp_mode = ULP_MODE_ISCSI;
		toep->ulpcb = icc;

		send_iscsi_flowc_wr(icc->sc, toep, CXGBEI_MAX_PDU);
		set_ulp_mode_iscsi(icc->sc, toep, ic->ic_header_crc32c,
		    ic->ic_data_crc32c);
		error = 0;
	}
	INP_WUNLOCK(inp);

	return (error);
}

void
icl_cxgbei_conn_close(struct icl_conn *ic)
{
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);
	struct icl_pdu *ip;
	struct socket *so;
	struct sockbuf *sb;
	struct inpcb *inp;
	struct toepcb *toep = icc->toep;

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);
	ICL_CONN_LOCK_ASSERT_NOT(ic);

	ICL_CONN_LOCK(ic);
	so = ic->ic_socket;
	if (ic->ic_disconnecting || so == NULL) {
		CTR4(KTR_CXGBE, "%s: icc %p (disconnecting = %d), so %p",
		    __func__, icc, ic->ic_disconnecting, so);
		ICL_CONN_UNLOCK(ic);
		return;
	}
	ic->ic_disconnecting = true;

	/* These are unused in this driver right now. */
	MPASS(STAILQ_EMPTY(&ic->ic_to_send));
	MPASS(ic->ic_receive_pdu == NULL);

#ifdef DIAGNOSTIC
	KASSERT(ic->ic_outstanding_pdus == 0,
	    ("destroying session with %d outstanding PDUs",
	     ic->ic_outstanding_pdus));
#endif
	ICL_CONN_UNLOCK(ic);

	CTR3(KTR_CXGBE, "%s: tid %d, icc %p", __func__, toep ? toep->tid : -1,
	    icc);
	inp = sotoinpcb(so);
	sb = &so->so_rcv;
	INP_WLOCK(inp);
	if (toep != NULL) {	/* NULL if connection was never offloaded. */
		toep->ulpcb = NULL;
		mbufq_drain(&toep->ulp_pduq);
		SOCKBUF_LOCK(sb);
		if (icc->rx_flags & RXF_ACTIVE) {
			volatile u_int *p = &icc->rx_flags;

			SOCKBUF_UNLOCK(sb);
			INP_WUNLOCK(inp);

			while (*p & RXF_ACTIVE)
				pause("conclo", 1);

			INP_WLOCK(inp);
			SOCKBUF_LOCK(sb);
		}

		while (!STAILQ_EMPTY(&icc->rcvd_pdus)) {
			ip = STAILQ_FIRST(&icc->rcvd_pdus);
			STAILQ_REMOVE_HEAD(&icc->rcvd_pdus, ip_next);
			icl_cxgbei_conn_pdu_free(ic, ip);
		}
		SOCKBUF_UNLOCK(sb);
	}
	INP_WUNLOCK(inp);

	ICL_CONN_LOCK(ic);
	ic->ic_socket = NULL;
	ICL_CONN_UNLOCK(ic);

	/*
	 * XXXNP: we should send RST instead of FIN when PDUs held in various
	 * queues were purged instead of delivered reliably but soabort isn't
	 * really general purpose and wouldn't do the right thing here.
	 */
	soclose(so);
}

int
icl_cxgbei_conn_task_setup(struct icl_conn *ic, struct icl_pdu *ip,
    struct ccb_scsiio *csio, uint32_t *task_tagp, void **prvp)
{
	void *prv;

	*task_tagp = icl_conn_build_tasktag(ic, *task_tagp);

	prv = uma_zalloc(icl_transfer_zone, M_NOWAIT | M_ZERO);
	if (prv == NULL)
		return (ENOMEM);

	*prvp = prv;

	cxgbei_conn_task_reserve_itt(ic, prvp, csio, task_tagp);

	return (0);
}

void
icl_cxgbei_conn_task_done(struct icl_conn *ic, void *prv)
{

	cxgbei_cleanup_task(ic, prv);
	uma_zfree(icl_transfer_zone, prv);
}

int
icl_cxgbei_conn_transfer_setup(struct icl_conn *ic, union ctl_io *io,
    uint32_t *transfer_tag, void **prvp)
{
	void *prv;

	*transfer_tag = icl_conn_build_tasktag(ic, *transfer_tag);

	prv = uma_zalloc(icl_transfer_zone, M_NOWAIT | M_ZERO);
	if (prv == NULL)
		return (ENOMEM);

	*prvp = prv;

	cxgbei_conn_transfer_reserve_ttt(ic, prvp, io, transfer_tag);

	return (0);
}

void
icl_cxgbei_conn_transfer_done(struct icl_conn *ic, void *prv)
{
	cxgbei_cleanup_task(ic, prv);
	uma_zfree(icl_transfer_zone, prv);
}

static int
icl_cxgbei_limits(size_t *limitp)
{

	*limitp = CXGBEI_MAX_DSL;

	return (0);
}

static int
icl_cxgbei_load(void)
{
	int error;

	icl_transfer_zone = uma_zcreate("icl_transfer",
	    16 * 1024, NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	refcount_init(&icl_cxgbei_ncons, 0);

	error = icl_register("cxgbei", false, -100, icl_cxgbei_limits,
	    icl_cxgbei_new_conn);
	KASSERT(error == 0, ("failed to register"));

	return (error);
}

static int
icl_cxgbei_unload(void)
{

	if (icl_cxgbei_ncons != 0)
		return (EBUSY);

	icl_unregister("cxgbei", false);

	uma_zdestroy(icl_transfer_zone);

	return (0);
}

static int
icl_cxgbei_modevent(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		return (icl_cxgbei_load());
	case MOD_UNLOAD:
		return (icl_cxgbei_unload());
	default:
		return (EINVAL);
	}
}

moduledata_t icl_cxgbei_data = {
	"icl_cxgbei",
	icl_cxgbei_modevent,
	0
};

DECLARE_MODULE(icl_cxgbei, icl_cxgbei_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_DEPEND(icl_cxgbei, icl, 1, 1, 1);
MODULE_VERSION(icl_cxgbei, 1);
#endif
