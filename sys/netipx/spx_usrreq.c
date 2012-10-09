/*-
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2004-2009 Robert N. M. Watson
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1995, Mike Mitchell
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)spx_usrreq.h
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/systm.h>

#include <net/route.h>
#include <netinet/tcp_fsm.h>

#include <netipx/ipx.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>
#include <netipx/spx.h>
#include <netipx/spx_debug.h>
#include <netipx/spx_timer.h>
#include <netipx/spx_var.h>

#include <security/mac/mac_framework.h>

/*
 * SPX protocol implementation.
 */
static struct	mtx spx_mtx;			/* Protects only spx_iss. */
static u_short 	spx_iss;
u_short		spx_newchecks[50];
static int	spx_hardnosed;
static int	traceallspxs = 0;
struct	spx_istat spx_istat;

#define	SPX_LOCK_INIT()	mtx_init(&spx_mtx, "spx_mtx", NULL, MTX_DEF)
#define	SPX_LOCK()	mtx_lock(&spx_mtx)
#define	SPX_UNLOCK()	mtx_unlock(&spx_mtx)

static const int spx_backoff[SPX_MAXRXTSHIFT+1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

static	void spx_close(struct spxpcb *cb);
static	void spx_disconnect(struct spxpcb *cb);
static	void spx_drop(struct spxpcb *cb, int errno);
static	void spx_setpersist(struct spxpcb *cb);
static	void spx_template(struct spxpcb *cb);
static	void spx_timers(struct spxpcb *cb, int timer);
static	void spx_usrclosed(struct spxpcb *cb);

static	void spx_usr_abort(struct socket *so);
static	int spx_accept(struct socket *so, struct sockaddr **nam);
static	int spx_attach(struct socket *so, int proto, struct thread *td);
static	int spx_bind(struct socket *so, struct sockaddr *nam, struct thread *td);
static	void spx_usr_close(struct socket *so);
static	int spx_connect(struct socket *so, struct sockaddr *nam,
			struct thread *td);
static	void spx_detach(struct socket *so);
static	void spx_pcbdetach(struct ipxpcb *ipxp);
static	int spx_usr_disconnect(struct socket *so);
static	int spx_listen(struct socket *so, int backlog, struct thread *td);
static	int spx_rcvd(struct socket *so, int flags);
static	int spx_rcvoob(struct socket *so, struct mbuf *m, int flags);
static	int spx_send(struct socket *so, int flags, struct mbuf *m,
		     struct sockaddr *addr, struct mbuf *control,
		     struct thread *td);
static	int spx_shutdown(struct socket *so);
static	int spx_sp_attach(struct socket *so, int proto, struct thread *td);

struct	pr_usrreqs spx_usrreqs = {
	.pru_abort =		spx_usr_abort,
	.pru_accept =		spx_accept,
	.pru_attach =		spx_attach,
	.pru_bind =		spx_bind,
	.pru_connect =		spx_connect,
	.pru_control =		ipx_control,
	.pru_detach =		spx_detach,
	.pru_disconnect =	spx_usr_disconnect,
	.pru_listen =		spx_listen,
	.pru_peeraddr =		ipx_peeraddr,
	.pru_rcvd =		spx_rcvd,
	.pru_rcvoob =		spx_rcvoob,
	.pru_send =		spx_send,
	.pru_shutdown =		spx_shutdown,
	.pru_sockaddr =		ipx_sockaddr,
	.pru_close =		spx_usr_close,
};

struct	pr_usrreqs spx_usrreq_sps = {
	.pru_abort =		spx_usr_abort,
	.pru_accept =		spx_accept,
	.pru_attach =		spx_sp_attach,
	.pru_bind =		spx_bind,
	.pru_connect =		spx_connect,
	.pru_control =		ipx_control,
	.pru_detach =		spx_detach,
	.pru_disconnect =	spx_usr_disconnect,
	.pru_listen =		spx_listen,
	.pru_peeraddr =		ipx_peeraddr,
	.pru_rcvd =		spx_rcvd,
	.pru_rcvoob =		spx_rcvoob,
	.pru_send =		spx_send,
	.pru_shutdown =		spx_shutdown,
	.pru_sockaddr =		ipx_sockaddr,
	.pru_close =		spx_usr_close,
};

void
spx_init(void)
{

	SPX_LOCK_INIT();
	spx_iss = 1; /* WRONG !! should fish it out of TODR */
}

void
spx_input(struct mbuf *m, struct ipxpcb *ipxp)
{
	struct spxpcb *cb;
	struct spx *si = mtod(m, struct spx *);
	struct socket *so;
	struct spx spx_savesi;
	int dropsocket = 0;
	short ostate = 0;

	spxstat.spxs_rcvtotal++;
	KASSERT(ipxp != NULL, ("spx_input: ipxpcb == NULL"));

	/*
	 * spx_input() assumes that the caller will hold both the pcb list
	 * lock and also the ipxp lock.  spx_input() will release both before
	 * returning, and may in fact trade in the ipxp lock for another pcb
	 * lock following sonewconn().
	 */
	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_input: cb == NULL"));

	if (ipxp->ipxp_flags & IPXP_DROPPED)
		goto drop;

	if (m->m_len < sizeof(*si)) {
		if ((m = m_pullup(m, sizeof(*si))) == NULL) {
			IPX_UNLOCK(ipxp);
			IPX_LIST_UNLOCK();
			spxstat.spxs_rcvshort++;
			return;
		}
		si = mtod(m, struct spx *);
	}
	si->si_seq = ntohs(si->si_seq);
	si->si_ack = ntohs(si->si_ack);
	si->si_alo = ntohs(si->si_alo);

	so = ipxp->ipxp_socket;
	KASSERT(so != NULL, ("spx_input: so == NULL"));

#ifdef MAC
	if (mac_socket_check_deliver(so, m) != 0)
		goto drop;
#endif

	if (so->so_options & SO_DEBUG || traceallspxs) {
		ostate = cb->s_state;
		spx_savesi = *si;
	}
	if (so->so_options & SO_ACCEPTCONN) {
		struct spxpcb *ocb = cb;

		so = sonewconn(so, 0);
		if (so == NULL)
			goto drop;

		/*
		 * This is ugly, but ....
		 *
		 * Mark socket as temporary until we're committed to keeping
		 * it.  The code at ``drop'' and ``dropwithreset'' check the
		 * flag dropsocket to see if the temporary socket created
		 * here should be discarded.  We mark the socket as
		 * discardable until we're committed to it below in
		 * TCPS_LISTEN.
		 *
		 * XXXRW: In the new world order of real kernel parallelism,
		 * temporarily allocating the socket when we're "not sure"
		 * seems like a bad idea, as we might race to remove it if
		 * the listen socket is closed...?
		 *
		 * We drop the lock of the listen socket ipxp, and acquire
		 * the lock of the new socket ippx.
		 */
		dropsocket++;
		IPX_UNLOCK(ipxp);
		ipxp = (struct ipxpcb *)so->so_pcb;
		IPX_LOCK(ipxp);
		ipxp->ipxp_laddr = si->si_dna;
		cb = ipxtospxpcb(ipxp);
		cb->s_mtu = ocb->s_mtu;		/* preserve sockopts */
		cb->s_flags = ocb->s_flags;	/* preserve sockopts */
		cb->s_flags2 = ocb->s_flags2;	/* preserve sockopts */
		cb->s_state = TCPS_LISTEN;
	}
	IPX_LOCK_ASSERT(ipxp);

	/*
	 * Packet received on connection.  Reset idle time and keep-alive
	 * timer.
	 */
	cb->s_idle = 0;
	cb->s_timer[SPXT_KEEP] = SPXTV_KEEP;

	switch (cb->s_state) {
	case TCPS_LISTEN:{
		struct sockaddr_ipx *sipx, ssipx;
		struct ipx_addr laddr;

		/*
		 * If somebody here was carying on a conversation and went
		 * away, and his pen pal thinks he can still talk, we get the
		 * misdirected packet.
		 */
		if (spx_hardnosed && (si->si_did != 0 || si->si_seq != 0)) {
			spx_istat.gonawy++;
			goto dropwithreset;
		}
		sipx = &ssipx;
		bzero(sipx, sizeof *sipx);
		sipx->sipx_len = sizeof(*sipx);
		sipx->sipx_family = AF_IPX;
		sipx->sipx_addr = si->si_sna;
		laddr = ipxp->ipxp_laddr;
		if (ipx_nullhost(laddr))
			ipxp->ipxp_laddr = si->si_dna;
		if (ipx_pcbconnect(ipxp, (struct sockaddr *)sipx, &thread0)) {
			ipxp->ipxp_laddr = laddr;
			spx_istat.noconn++;
			goto drop;
		}
		spx_template(cb);
		dropsocket = 0;		/* committed to socket */
		cb->s_did = si->si_sid;
		cb->s_rack = si->si_ack;
		cb->s_ralo = si->si_alo;
#define THREEWAYSHAKE
#ifdef THREEWAYSHAKE
		cb->s_state = TCPS_SYN_RECEIVED;
		cb->s_force = 1 + SPXT_KEEP;
		spxstat.spxs_accepts++;
		cb->s_timer[SPXT_KEEP] = SPXTV_KEEP;
		}
		break;

	 case TCPS_SYN_RECEIVED: {
		/*
		 * This state means that we have heard a response to our
		 * acceptance of their connection.  It is probably logically
		 * unnecessary in this implementation.
		 */
		if (si->si_did != cb->s_sid) {
			spx_istat.wrncon++;
			goto drop;
		}
#endif
		ipxp->ipxp_fport =  si->si_sport;
		cb->s_timer[SPXT_REXMT] = 0;
		cb->s_timer[SPXT_KEEP] = SPXTV_KEEP;
		soisconnected(so);
		cb->s_state = TCPS_ESTABLISHED;
		spxstat.spxs_accepts++;
		}
		break;

	case TCPS_SYN_SENT:
		/*
		 * This state means that we have gotten a response to our
		 * attempt to establish a connection.  We fill in the data
		 * from the other side, telling us which port to respond to,
		 * instead of the well-known one we might have sent to in the
		 * first place.  We also require that this is a response to
		 * our connection id.
		 */
		if (si->si_did != cb->s_sid) {
			spx_istat.notme++;
			goto drop;
		}
		spxstat.spxs_connects++;
		cb->s_did = si->si_sid;
		cb->s_rack = si->si_ack;
		cb->s_ralo = si->si_alo;
		cb->s_dport = ipxp->ipxp_fport =  si->si_sport;
		cb->s_timer[SPXT_REXMT] = 0;
		cb->s_flags |= SF_ACKNOW;
		soisconnected(so);
		cb->s_state = TCPS_ESTABLISHED;

		/*
		 * Use roundtrip time of connection request for initial rtt.
		 */
		if (cb->s_rtt) {
			cb->s_srtt = cb->s_rtt << 3;
			cb->s_rttvar = cb->s_rtt << 1;
			SPXT_RANGESET(cb->s_rxtcur,
			    ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1,
			    SPXTV_MIN, SPXTV_REXMTMAX);
			    cb->s_rtt = 0;
		}
	}

	if (so->so_options & SO_DEBUG || traceallspxs)
		spx_trace(SA_INPUT, (u_char)ostate, cb, &spx_savesi, 0);

	m->m_len -= sizeof(struct ipx);
	m->m_pkthdr.len -= sizeof(struct ipx);
	m->m_data += sizeof(struct ipx);

	if (spx_reass(cb, m, si))
		m_freem(m);
	if (cb->s_force || (cb->s_flags & (SF_ACKNOW|SF_WIN|SF_RXT)))
		spx_output(cb, NULL);
	cb->s_flags &= ~(SF_WIN|SF_RXT);
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return;

dropwithreset:
	IPX_LOCK_ASSERT(ipxp);
	if (cb == NULL || (cb->s_ipxpcb->ipxp_socket->so_options & SO_DEBUG ||
	    traceallspxs))
		spx_trace(SA_DROP, (u_char)ostate, cb, &spx_savesi, 0);
	IPX_UNLOCK(ipxp);
	if (dropsocket) {
		struct socket *head;
		ACCEPT_LOCK();
		KASSERT((so->so_qstate & SQ_INCOMP) != 0,
		    ("spx_input: nascent socket not SQ_INCOMP on soabort()"));
		head = so->so_head;
		TAILQ_REMOVE(&head->so_incomp, so, so_list);
		head->so_incqlen--;
		so->so_qstate &= ~SQ_INCOMP;
		so->so_head = NULL;
		ACCEPT_UNLOCK();
		soabort(so);
	}
	IPX_LIST_UNLOCK();
	m_freem(m);
	return;

drop:
	IPX_LOCK_ASSERT(ipxp);
	if (cb->s_ipxpcb->ipxp_socket->so_options & SO_DEBUG || traceallspxs)
		spx_trace(SA_DROP, (u_char)ostate, cb, &spx_savesi, 0);
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	m_freem(m);
}

void
spx_ctlinput(int cmd, struct sockaddr *arg_as_sa, void *dummy)
{

	/* Currently, nothing. */
}

int
spx_output(struct spxpcb *cb, struct mbuf *m0)
{
	struct socket *so = cb->s_ipxpcb->ipxp_socket;
	struct mbuf *m = NULL;
	struct spx *si = NULL;
	struct sockbuf *sb = &so->so_snd;
	int len = 0, win, rcv_win;
	short span, off, recordp = 0;
	u_short alo;
	int error = 0, sendalot;
#ifdef notdef
	int idle;
#endif
	struct mbuf *mprev;

	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	if (m0 != NULL) {
		int mtu = cb->s_mtu;
		int datalen;

		/*
		 * Make sure that packet isn't too big.
		 */
		for (m = m0; m != NULL; m = m->m_next) {
			mprev = m;
			len += m->m_len;
			if (m->m_flags & M_EOR)
				recordp = 1;
		}
		datalen = (cb->s_flags & SF_HO) ?
				len - sizeof(struct spxhdr) : len;
		if (datalen > mtu) {
			if (cb->s_flags & SF_PI) {
				m_freem(m0);
				return (EMSGSIZE);
			} else {
				int oldEM = cb->s_cc & SPX_EM;

				cb->s_cc &= ~SPX_EM;
				while (len > mtu) {
					m = m_copym(m0, 0, mtu, M_DONTWAIT);
					if (m == NULL) {
					    cb->s_cc |= oldEM;
					    m_freem(m0);
					    return (ENOBUFS);
					}
					if (cb->s_flags & SF_NEWCALL) {
					    struct mbuf *mm = m;
					    spx_newchecks[7]++;
					    while (mm != NULL) {
						mm->m_flags &= ~M_EOR;
						mm = mm->m_next;
					    }
					}
					error = spx_output(cb, m);
					if (error) {
						cb->s_cc |= oldEM;
						m_freem(m0);
						return (error);
					}
					m_adj(m0, mtu);
					len -= mtu;
				}
				cb->s_cc |= oldEM;
			}
		}

		/*
		 * Force length even, by adding a "garbage byte" if
		 * necessary.
		 */
		if (len & 1) {
			m = mprev;
			if (M_TRAILINGSPACE(m) >= 1)
				m->m_len++;
			else {
				struct mbuf *m1 = m_get(M_DONTWAIT, MT_DATA);

				if (m1 == NULL) {
					m_freem(m0);
					return (ENOBUFS);
				}
				m1->m_len = 1;
				*(mtod(m1, u_char *)) = 0;
				m->m_next = m1;
			}
		}
		m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			m_freem(m0);
			return (ENOBUFS);
		}

		/*
		 * Fill in mbuf with extended SP header and addresses and
		 * length put into network format.
		 */
		MH_ALIGN(m, sizeof(struct spx));
		m->m_len = sizeof(struct spx);
		m->m_next = m0;
		si = mtod(m, struct spx *);
		si->si_i = cb->s_ipx;
		si->si_s = cb->s_shdr;
		if ((cb->s_flags & SF_PI) && (cb->s_flags & SF_HO)) {
			struct spxhdr *sh;
			if (m0->m_len < sizeof(*sh)) {
				if((m0 = m_pullup(m0, sizeof(*sh))) == NULL) {
					m_free(m);
					m_freem(m0);
					return (EINVAL);
				}
				m->m_next = m0;
			}
			sh = mtod(m0, struct spxhdr *);
			si->si_dt = sh->spx_dt;
			si->si_cc |= sh->spx_cc & SPX_EM;
			m0->m_len -= sizeof(*sh);
			m0->m_data += sizeof(*sh);
			len -= sizeof(*sh);
		}
		len += sizeof(*si);
		if ((cb->s_flags2 & SF_NEWCALL) && recordp) {
			si->si_cc |= SPX_EM;
			spx_newchecks[8]++;
		}
		if (cb->s_oobflags & SF_SOOB) {
			/*
			 * Per jqj@cornell: Make sure OB packets convey
			 * exactly 1 byte.  If the packet is 1 byte or
			 * larger, we have already guaranted there to be at
			 * least one garbage byte for the checksum, and extra
			 * bytes shouldn't hurt!
			 */
			if (len > sizeof(*si)) {
				si->si_cc |= SPX_OB;
				len = (1 + sizeof(*si));
			}
		}
		si->si_len = htons((u_short)len);
		m->m_pkthdr.len = ((len - 1) | 1) + 1;

		/*
		 * Queue stuff up for output.
		 */
		sbappendrecord(sb, m);
		cb->s_seq++;
	}
#ifdef notdef
	idle = (cb->s_smax == (cb->s_rack - 1));
#endif
again:
	sendalot = 0;
	off = cb->s_snxt - cb->s_rack;
	win = min(cb->s_swnd, (cb->s_cwnd / CUNIT));

	/*
	 * If in persist timeout with window of 0, send a probe.  Otherwise,
	 * if window is small but non-zero and timer expired, send what we
	 * can and go into transmit state.
	 */
	if (cb->s_force == 1 + SPXT_PERSIST) {
		if (win != 0) {
			cb->s_timer[SPXT_PERSIST] = 0;
			cb->s_rxtshift = 0;
		}
	}
	span = cb->s_seq - cb->s_rack;
	len = min(span, win) - off;

	if (len < 0) {
		/*
		 * Window shrank after we went into it.  If window shrank to
		 * 0, cancel pending restransmission and pull s_snxt back to
		 * (closed) window.  We will enter persist state below.  If
		 * the widndow didn't close completely, just wait for an ACK.
		 */
		len = 0;
		if (win == 0) {
			cb->s_timer[SPXT_REXMT] = 0;
			cb->s_snxt = cb->s_rack;
		}
	}
	if (len > 1)
		sendalot = 1;
	rcv_win = sbspace(&so->so_rcv);

	/*
	 * Send if we owe peer an ACK.
	 */
	if (cb->s_oobflags & SF_SOOB) {
		/*
		 * Must transmit this out of band packet.
		 */
		cb->s_oobflags &= ~ SF_SOOB;
		sendalot = 1;
		spxstat.spxs_sndurg++;
		goto found;
	}
	if (cb->s_flags & SF_ACKNOW)
		goto send;
	if (cb->s_state < TCPS_ESTABLISHED)
		goto send;

	/*
	 * Silly window can't happen in spx.  Code from TCP deleted.
	 */
	if (len)
		goto send;

	/*
	 * Compare available window to amount of window known to peer (as
	 * advertised window less next expected input.)  If the difference is
	 * at least two packets or at least 35% of the mximum possible
	 * window, then want to send a window update to peer.
	 */
	if (rcv_win > 0) {
		u_short delta =  1 + cb->s_alo - cb->s_ack;
		int adv = rcv_win - (delta * cb->s_mtu);

		if ((so->so_rcv.sb_cc == 0 && adv >= (2 * cb->s_mtu)) ||
		    (100 * adv / so->so_rcv.sb_hiwat >= 35)) {
			spxstat.spxs_sndwinup++;
			cb->s_flags |= SF_ACKNOW;
			goto send;
		}

	}

	/*
	 * Many comments from tcp_output.c are appropriate here including ...
	 * If send window is too small, there is data to transmit, and no
	 * retransmit or persist is pending, then go to persist state.  If
	 * nothing happens soon, send when timer expires: if window is
	 * non-zero, transmit what we can, otherwise send a probe.
	 */
	if (so->so_snd.sb_cc && cb->s_timer[SPXT_REXMT] == 0 &&
	    cb->s_timer[SPXT_PERSIST] == 0) {
		cb->s_rxtshift = 0;
		spx_setpersist(cb);
	}

	/*
	 * No reason to send a packet, just return.
	 */
	cb->s_outx = 1;
	return (0);

send:
	/*
	 * Find requested packet.
	 */
	si = NULL;
	m = NULL;
	if (len > 0) {
		cb->s_want = cb->s_snxt;
		for (m = sb->sb_mb; m != NULL; m = m->m_nextpkt) {
			si = mtod(m, struct spx *);
			if (SSEQ_LEQ(cb->s_snxt, si->si_seq))
				break;
		}
	found:
		if (si != NULL) {
			if (si->si_seq != cb->s_snxt) {
				spxstat.spxs_sndvoid++;
				si = NULL;
				m = NULL;
			} else
				cb->s_snxt++;
		}
	}

	/*
	 * Update window.
	 */
	if (rcv_win < 0)
		rcv_win = 0;
	alo = cb->s_ack - 1 + (rcv_win / ((short)cb->s_mtu));
	if (SSEQ_LT(alo, cb->s_alo))
		alo = cb->s_alo;

	if (m != NULL) {
		/*
		 * Must make a copy of this packet for ipx_output to monkey
		 * with.
		 */
		m = m_copy(m, 0, M_COPYALL);
		if (m == NULL)
			return (ENOBUFS);
		si = mtod(m, struct spx *);
		if (SSEQ_LT(si->si_seq, cb->s_smax))
			spxstat.spxs_sndrexmitpack++;
		else
			spxstat.spxs_sndpack++;
	} else if (cb->s_force || cb->s_flags & SF_ACKNOW) {
		/*
		 * Must send an acknowledgement or a probe.
		 */
		if (cb->s_force)
			spxstat.spxs_sndprobe++;
		if (cb->s_flags & SF_ACKNOW)
			spxstat.spxs_sndacks++;
		m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);

		/*
		 * Fill in mbuf with extended SP header and addresses and
		 * length put into network format.
		 */
		MH_ALIGN(m, sizeof(struct spx));
		m->m_len = sizeof(*si);
		m->m_pkthdr.len = sizeof(*si);
		si = mtod(m, struct spx *);
		si->si_i = cb->s_ipx;
		si->si_s = cb->s_shdr;
		si->si_seq = cb->s_smax + 1;
		si->si_len = htons(sizeof(*si));
		si->si_cc |= SPX_SP;
	} else {
		cb->s_outx = 3;
		if (so->so_options & SO_DEBUG || traceallspxs)
			spx_trace(SA_OUTPUT, cb->s_state, cb, si, 0);
		return (0);
	}

	/*
	 * Stuff checksum and output datagram.
	 */
	if ((si->si_cc & SPX_SP) == 0) {
		if (cb->s_force != (1 + SPXT_PERSIST) ||
		    cb->s_timer[SPXT_PERSIST] == 0) {
			/*
			 * If this is a new packet and we are not currently
			 * timing anything, time this one.
			 */
			if (SSEQ_LT(cb->s_smax, si->si_seq)) {
				cb->s_smax = si->si_seq;
				if (cb->s_rtt == 0) {
					spxstat.spxs_segstimed++;
					cb->s_rtseq = si->si_seq;
					cb->s_rtt = 1;
				}
			}

			/*
			 * Set rexmt timer if not currently set, initial
			 * value for retransmit timer is smoothed round-trip
			 * time + 2 * round-trip time variance.  Initialize
			 * shift counter which is used for backoff of
			 * retransmit time.
			 */
			if (cb->s_timer[SPXT_REXMT] == 0 &&
			    cb->s_snxt != cb->s_rack) {
				cb->s_timer[SPXT_REXMT] = cb->s_rxtcur;
				if (cb->s_timer[SPXT_PERSIST]) {
					cb->s_timer[SPXT_PERSIST] = 0;
					cb->s_rxtshift = 0;
				}
			}
		} else if (SSEQ_LT(cb->s_smax, si->si_seq))
			cb->s_smax = si->si_seq;
	} else if (cb->s_state < TCPS_ESTABLISHED) {
		if (cb->s_rtt == 0)
			cb->s_rtt = 1; /* Time initial handshake */
		if (cb->s_timer[SPXT_REXMT] == 0)
			cb->s_timer[SPXT_REXMT] = cb->s_rxtcur;
	}

	/*
	 * Do not request acks when we ack their data packets or when we do a
	 * gratuitous window update.
	 */
	if (((si->si_cc & SPX_SP) == 0) || cb->s_force)
		si->si_cc |= SPX_SA;
	si->si_seq = htons(si->si_seq);
	si->si_alo = htons(alo);
	si->si_ack = htons(cb->s_ack);

	if (ipxcksum)
		si->si_sum = ipx_cksum(m, ntohs(si->si_len));
	else
		si->si_sum = 0xffff;

	cb->s_outx = 4;
	if (so->so_options & SO_DEBUG || traceallspxs)
		spx_trace(SA_OUTPUT, cb->s_state, cb, si, 0);

#ifdef MAC
	mac_socket_create_mbuf(so, m);
#endif

	if (so->so_options & SO_DONTROUTE)
		error = ipx_outputfl(m, NULL, IPX_ROUTETOIF);
	else
		error = ipx_outputfl(m, &cb->s_ipxpcb->ipxp_route, 0);
	if (error)
		return (error);
	spxstat.spxs_sndtotal++;

	/*
	 * Data sent (as far as we can tell).  If this advertises a larger
	 * window than any other segment, then remember the size of the
	 * advertized window.  Any pending ACK has now been sent.
	 */
	cb->s_force = 0;
	cb->s_flags &= ~(SF_ACKNOW|SF_DELACK);
	if (SSEQ_GT(alo, cb->s_alo))
		cb->s_alo = alo;
	if (sendalot)
		goto again;
	cb->s_outx = 5;
	return (0);
}

static int spx_do_persist_panics = 0;

static void
spx_setpersist(struct spxpcb *cb)
{
	int t = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;

	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	if (cb->s_timer[SPXT_REXMT] && spx_do_persist_panics)
		panic("spx_output REXMT");

	/*
	 * Start/restart persistance timer.
	 */
	SPXT_RANGESET(cb->s_timer[SPXT_PERSIST],
	    t*spx_backoff[cb->s_rxtshift],
	    SPXTV_PERSMIN, SPXTV_PERSMAX);
	if (cb->s_rxtshift < SPX_MAXRXTSHIFT)
		cb->s_rxtshift++;
}

int
spx_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct spxhdr spxhdr;
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int mask, error;
	short soptval;
	u_short usoptval;
	int optval;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_ctloutput: ipxp == NULL"));

	/*
	 * This will have to be changed when we do more general stacking of
	 * protocols.
	 */
	if (sopt->sopt_level != IPXPROTO_SPX)
		return (ipx_ctloutput(so, sopt));

	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		IPX_UNLOCK(ipxp);
		return (ECONNRESET);
	}

	IPX_LOCK(ipxp);
	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_ctloutput: cb == NULL"));

	error = 0;
	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case SO_HEADERS_ON_INPUT:
			mask = SF_HI;
			goto get_flags;

		case SO_HEADERS_ON_OUTPUT:
			mask = SF_HO;
		get_flags:
			soptval = cb->s_flags & mask;
			IPX_UNLOCK(ipxp);
			error = sooptcopyout(sopt, &soptval,
			    sizeof(soptval));
			break;

		case SO_MTU:
			usoptval = cb->s_mtu;
			IPX_UNLOCK(ipxp);
			error = sooptcopyout(sopt, &usoptval,
			    sizeof(usoptval));
			break;

		case SO_LAST_HEADER:
			spxhdr = cb->s_rhdr;
			IPX_UNLOCK(ipxp);
			error = sooptcopyout(sopt, &spxhdr, sizeof(spxhdr));
			break;

		case SO_DEFAULT_HEADERS:
			spxhdr = cb->s_shdr;
			IPX_UNLOCK(ipxp);
			error = sooptcopyout(sopt, &spxhdr, sizeof(spxhdr));
			break;

		default:
			IPX_UNLOCK(ipxp);
			error = ENOPROTOOPT;
		}
		break;

	case SOPT_SET:
		/*
		 * XXX Why are these shorts on get and ints on set?  That
		 * doesn't make any sense...
		 *
		 * XXXRW: Note, when we re-acquire the ipxp lock, we should
		 * re-check that it's not dropped.
		 */
		IPX_UNLOCK(ipxp);
		switch (sopt->sopt_name) {
		case SO_HEADERS_ON_INPUT:
			mask = SF_HI;
			goto set_head;

		case SO_HEADERS_ON_OUTPUT:
			mask = SF_HO;
		set_head:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			IPX_LOCK(ipxp);
			if (cb->s_flags & SF_PI) {
				if (optval)
					cb->s_flags |= mask;
				else
					cb->s_flags &= ~mask;
			} else error = EINVAL;
			IPX_UNLOCK(ipxp);
			break;

		case SO_MTU:
			error = sooptcopyin(sopt, &usoptval, sizeof usoptval,
					    sizeof usoptval);
			if (error)
				break;
			/* Unlocked write. */
			cb->s_mtu = usoptval;
			break;

#ifdef SF_NEWCALL
		case SO_NEWCALL:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;
			IPX_LOCK(ipxp);
			if (optval) {
				cb->s_flags2 |= SF_NEWCALL;
				spx_newchecks[5]++;
			} else {
				cb->s_flags2 &= ~SF_NEWCALL;
				spx_newchecks[6]++;
			}
			IPX_UNLOCK(ipxp);
			break;
#endif

		case SO_DEFAULT_HEADERS:
			{
				struct spxhdr sp;

				error = sooptcopyin(sopt, &sp, sizeof sp,
						    sizeof sp);
				if (error)
					break;
				IPX_LOCK(ipxp);
				cb->s_dt = sp.spx_dt;
				cb->s_cc = sp.spx_cc & SPX_EM;
				IPX_UNLOCK(ipxp);
			}
			break;

		default:
			error = ENOPROTOOPT;
		}
		break;

	default:
		panic("spx_ctloutput: bad socket option direction");
	}
	return (error);
}

static void
spx_usr_abort(struct socket *so)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_usr_abort: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_usr_abort: cb == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	spx_drop(cb, ECONNABORTED);
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
}

/*
 * Accept a connection.  Essentially all the work is done at higher levels;
 * just return the address of the peer, storing through addr.
 */
static int
spx_accept(struct socket *so, struct sockaddr **nam)
{
	struct ipxpcb *ipxp;
	struct sockaddr_ipx *sipx, ssipx;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_accept: ipxp == NULL"));

	sipx = &ssipx;
	bzero(sipx, sizeof *sipx);
	sipx->sipx_len = sizeof *sipx;
	sipx->sipx_family = AF_IPX;
	IPX_LOCK(ipxp);
	sipx->sipx_addr = ipxp->ipxp_faddr;
	IPX_UNLOCK(ipxp);
	*nam = sodupsockaddr((struct sockaddr *)sipx, M_WAITOK);
	return (0);
}

static int
spx_attach(struct socket *so, int proto, struct thread *td)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	struct mbuf *mm;
	struct sockbuf *sb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp == NULL, ("spx_attach: ipxp != NULL"));

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, (u_long) 3072, (u_long) 3072);
		if (error)
			return (error);
	}

	cb = malloc(sizeof *cb, M_PCB, M_NOWAIT | M_ZERO);
	if (cb == NULL)
		return (ENOBUFS);
	mm = m_getclr(M_DONTWAIT, MT_DATA);
	if (mm == NULL) {
		free(cb, M_PCB);
		return (ENOBUFS);
	}

	IPX_LIST_LOCK();
	error = ipx_pcballoc(so, &ipxpcb_list, td);
	if (error) {
		IPX_LIST_UNLOCK();
		m_free(mm);
		free(cb, M_PCB);
		return (error);
	}
	ipxp = sotoipxpcb(so);
	ipxp->ipxp_flags |= IPXP_SPX;

	cb->s_state = TCPS_LISTEN;
	cb->s_smax = -1;
	cb->s_swl1 = -1;
	spx_reass_init(cb);
	cb->s_ipxpcb = ipxp;
	cb->s_mtu = 576 - sizeof(struct spx);
	sb = &so->so_snd;
	cb->s_cwnd = sbspace(sb) * CUNIT / cb->s_mtu;
	cb->s_ssthresh = cb->s_cwnd;
	cb->s_cwmx = sbspace(sb) * CUNIT / (2 * sizeof(struct spx));

	/*
	 * Above is recomputed when connecting to account for changed
	 * buffering or mtu's.
	 */
	cb->s_rtt = SPXTV_SRTTBASE;
	cb->s_rttvar = SPXTV_SRTTDFLT << 2;
	SPXT_RANGESET(cb->s_rxtcur,
	    ((SPXTV_SRTTBASE >> 2) + (SPXTV_SRTTDFLT << 2)) >> 1,
	    SPXTV_MIN, SPXTV_REXMTMAX);
	ipxp->ipxp_pcb = (caddr_t)cb;
	IPX_LIST_UNLOCK();
	return (0);
}

static void
spx_pcbdetach(struct ipxpcb *ipxp)
{
	struct spxpcb *cb;

	IPX_LOCK_ASSERT(ipxp);

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_pcbdetach: cb == NULL"));

	spx_reass_flush(cb);
	free(cb, M_PCB);
	ipxp->ipxp_pcb = NULL;
}

static int
spx_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ipxpcb *ipxp;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_bind: ipxp == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto out;
	}
	error = ipx_pcbbind(ipxp, nam, td);
out:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

static void
spx_usr_close(struct socket *so)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_usr_close: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_usr_close: cb == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (cb->s_state > TCPS_LISTEN)
		spx_disconnect(cb);
	else
		spx_close(cb);
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
}

/*
 * Initiate connection to peer.  Enter SYN_SENT state, and mark socket as
 * connecting.  Start keep-alive timer, setup prototype header, send initial
 * system packet requesting connection.
 */
static int
spx_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_connect: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_connect: cb == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto spx_connect_end;
	}
	if (ipxp->ipxp_lport == 0) {
		error = ipx_pcbbind(ipxp, NULL, td);
		if (error)
			goto spx_connect_end;
	}
	error = ipx_pcbconnect(ipxp, nam, td);
	if (error)
		goto spx_connect_end;
	soisconnecting(so);
	spxstat.spxs_connattempt++;
	cb->s_state = TCPS_SYN_SENT;
	cb->s_did = 0;
	spx_template(cb);
	cb->s_timer[SPXT_KEEP] = SPXTV_KEEP;
	cb->s_force = 1 + SPXTV_KEEP;

	/*
	 * Other party is required to respond to the port I send from, but he
	 * is not required to answer from where I am sending to, so allow
	 * wildcarding.  Original port I am sending to is still saved in
	 * cb->s_dport.
	 */
	ipxp->ipxp_fport = 0;
	error = spx_output(cb, NULL);
spx_connect_end:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

static void
spx_detach(struct socket *so)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;

	/*
	 * XXXRW: Should assert appropriately detached.
	 */
	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_detach: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_detach: cb == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	spx_pcbdetach(ipxp);
	ipx_pcbdetach(ipxp);
	ipx_pcbfree(ipxp);
	IPX_LIST_UNLOCK();
}

/*
 * We may decide later to implement connection closing handshaking at the spx
 * level optionally.  Here is the hook to do it:
 */
static int
spx_usr_disconnect(struct socket *so)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_usr_disconnect: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_usr_disconnect: cb == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto out;
	}
	spx_disconnect(cb);
	error = 0;
out:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

static int
spx_listen(struct socket *so, int backlog, struct thread *td)
{
	int error;
	struct ipxpcb *ipxp;
	struct spxpcb *cb;

	error = 0;
	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_listen: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_listen: cb == NULL"));

	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto out;
	}
	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0 && ipxp->ipxp_lport == 0)
		error = ipx_pcbbind(ipxp, NULL, td);
	if (error == 0) {
		cb->s_state = TCPS_LISTEN;
		solisten_proto(so, backlog);
	}
	SOCK_UNLOCK(so);
out:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

/*
 * After a receive, possibly send acknowledgment updating allocation.
 */
static int
spx_rcvd(struct socket *so, int flags)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_rcvd: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_rcvd: cb == NULL"));

	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto out;
	}
	cb->s_flags |= SF_RVD;
	spx_output(cb, NULL);
	cb->s_flags &= ~SF_RVD;
	error = 0;
out:
	IPX_UNLOCK(ipxp);
	return (error);
}

static int
spx_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_rcvoob: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_rcvoob: cb == NULL"));

	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto out;
	}
	SOCKBUF_LOCK(&so->so_rcv);
	if ((cb->s_oobflags & SF_IOOB) || so->so_oobmark ||
	    (so->so_rcv.sb_state & SBS_RCVATMARK)) {
		SOCKBUF_UNLOCK(&so->so_rcv);
		m->m_len = 1;
		*mtod(m, caddr_t) = cb->s_iobc;
		error = 0;
		goto out;
	}
	SOCKBUF_UNLOCK(&so->so_rcv);
	error = EINVAL;
out:
	IPX_UNLOCK(ipxp);
	return (error);
}

static int
spx_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *controlp, struct thread *td)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_send: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_send: cb == NULL"));

	error = 0;
	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = ECONNRESET;
		goto spx_send_end;
	}
	if (flags & PRUS_OOB) {
		if (sbspace(&so->so_snd) < -512) {
			error = ENOBUFS;
			goto spx_send_end;
		}
		cb->s_oobflags |= SF_SOOB;
	}
	if (controlp != NULL) {
		u_short *p = mtod(controlp, u_short *);
		spx_newchecks[2]++;
		if ((p[0] == 5) && (p[1] == 1)) { /* XXXX, for testing */
			cb->s_shdr.spx_dt = *(u_char *)(&p[2]);
			spx_newchecks[3]++;
		}
		m_freem(controlp);
	}
	controlp = NULL;
	error = spx_output(cb, m);
	m = NULL;
spx_send_end:
	IPX_UNLOCK(ipxp);
	if (controlp != NULL)
		m_freem(controlp);
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
spx_shutdown(struct socket *so)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_shutdown: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_shutdown: cb == NULL"));

	socantsendmore(so);
	IPX_LIST_LOCK();
	IPX_LOCK(ipxp);
	if (ipxp->ipxp_flags & IPXP_DROPPED) {
		error = EINVAL;
		goto out;
	}
	spx_usrclosed(cb);
	error = 0;
out:
	IPX_UNLOCK(ipxp);
	IPX_LIST_UNLOCK();
	return (error);
}

static int
spx_sp_attach(struct socket *so, int proto, struct thread *td)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int error;

	KASSERT(so->so_pcb == NULL, ("spx_sp_attach: so_pcb != NULL"));

	error = spx_attach(so, proto, td);
	if (error)
		return (error);

	ipxp = sotoipxpcb(so);
	KASSERT(ipxp != NULL, ("spx_sp_attach: ipxp == NULL"));

	cb = ipxtospxpcb(ipxp);
	KASSERT(cb != NULL, ("spx_sp_attach: cb == NULL"));

	IPX_LOCK(ipxp);
	cb->s_flags |= (SF_HI | SF_HO | SF_PI);
	IPX_UNLOCK(ipxp);
	return (0);
}

/*
 * Create template to be used to send spx packets on a connection.  Called
 * after host entry created, fills in a skeletal spx header (choosing
 * connection id), minimizing the amount of work necessary when the
 * connection is used.
 */
static void
spx_template(struct spxpcb *cb)
{
	struct ipxpcb *ipxp = cb->s_ipxpcb;
	struct sockbuf *sb = &(ipxp->ipxp_socket->so_snd);

	IPX_LOCK_ASSERT(ipxp);

	cb->s_ipx.ipx_pt = IPXPROTO_SPX;
	cb->s_ipx.ipx_sna = ipxp->ipxp_laddr;
	cb->s_ipx.ipx_dna = ipxp->ipxp_faddr;
	SPX_LOCK();
	cb->s_sid = htons(spx_iss);
	spx_iss += SPX_ISSINCR/2;
	SPX_UNLOCK();
	cb->s_alo = 1;
	cb->s_cwnd = (sbspace(sb) * CUNIT) / cb->s_mtu;

	/*
	 * Try to expand fast to full complement of large packets.
	 */
	cb->s_ssthresh = cb->s_cwnd;
	cb->s_cwmx = (sbspace(sb) * CUNIT) / (2 * sizeof(struct spx));

	/*
	 * But allow for lots of little packets as well.
	 */
	cb->s_cwmx = max(cb->s_cwmx, cb->s_cwnd);
}

/*
 * Close a SPIP control block.  Wake up any sleepers.  We used to free any
 * queued packets, but now we defer that until the pcb is discarded.
 */
void
spx_close(struct spxpcb *cb)
{
	struct ipxpcb *ipxp = cb->s_ipxpcb;
	struct socket *so = ipxp->ipxp_socket;

	KASSERT(ipxp != NULL, ("spx_close: ipxp == NULL"));
	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(ipxp);

	ipxp->ipxp_flags |= IPXP_DROPPED;
	soisdisconnected(so);
	spxstat.spxs_closed++;
}

/*
 * Someday we may do level 3 handshaking to close a connection or send a
 * xerox style error.  For now, just close.  cb will always be invalid after
 * this call.
 */
static void
spx_usrclosed(struct spxpcb *cb)
{

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	spx_close(cb);
}

/*
 * cb will always be invalid after this call.
 */
static void
spx_disconnect(struct spxpcb *cb)
{

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	spx_close(cb);
}

/*
 * Drop connection, reporting the specified error.  cb will always be invalid
 * after this call.
 */
static void
spx_drop(struct spxpcb *cb, int errno)
{
	struct socket *so = cb->s_ipxpcb->ipxp_socket;

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	/*
	 * Someday, in the xerox world we will generate error protocol
	 * packets announcing that the socket has gone away.
	 */
	if (TCPS_HAVERCVDSYN(cb->s_state)) {
		spxstat.spxs_drops++;
		cb->s_state = TCPS_CLOSED;
		/*tcp_output(cb);*/
	} else
		spxstat.spxs_conndrops++;
	so->so_error = errno;
	spx_close(cb);
}

/*
 * Fast timeout routine for processing delayed acks.
 */
void
spx_fasttimo(void)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;

	IPX_LIST_LOCK();
	LIST_FOREACH(ipxp, &ipxpcb_list, ipxp_list) {
		IPX_LOCK(ipxp);
		if (!(ipxp->ipxp_flags & IPXP_SPX) ||
		    (ipxp->ipxp_flags & IPXP_DROPPED)) {
			IPX_UNLOCK(ipxp);
			continue;
		}
		cb = ipxtospxpcb(ipxp);
		if (cb->s_flags & SF_DELACK) {
			cb->s_flags &= ~SF_DELACK;
			cb->s_flags |= SF_ACKNOW;
			spxstat.spxs_delack++;
			spx_output(cb, NULL);
		}
		IPX_UNLOCK(ipxp);
	}
	IPX_LIST_UNLOCK();
}

/*
 * spx protocol timeout routine called every 500 ms.  Updates the timers in
 * all active pcb's and causes finite state machine actions if timers expire.
 */
void
spx_slowtimo(void)
{
	struct ipxpcb *ipxp;
	struct spxpcb *cb;
	int i;

	/*
	 * Search through tcb's and update active timers.  Once, timers could
	 * free ipxp's, but now we do that only when detaching a socket.
	 */
	IPX_LIST_LOCK();
	LIST_FOREACH(ipxp, &ipxpcb_list, ipxp_list) {
		IPX_LOCK(ipxp);
		if (!(ipxp->ipxp_flags & IPXP_SPX) ||
		    (ipxp->ipxp_flags & IPXP_DROPPED)) {
			IPX_UNLOCK(ipxp);
			continue;
		}

		cb = (struct spxpcb *)ipxp->ipxp_pcb;
		KASSERT(cb != NULL, ("spx_slowtimo: cb == NULL"));
		for (i = 0; i < SPXT_NTIMERS; i++) {
			if (cb->s_timer[i] && --cb->s_timer[i] == 0) {
				spx_timers(cb, i);
				if (ipxp->ipxp_flags & IPXP_DROPPED)
					break;
			}
		}
		if (!(ipxp->ipxp_flags & IPXP_DROPPED)) {
			cb->s_idle++;
			if (cb->s_rtt)
				cb->s_rtt++;
		}
		IPX_UNLOCK(ipxp);
	}
	IPX_LIST_UNLOCK();
	SPX_LOCK();
	spx_iss += SPX_ISSINCR/PR_SLOWHZ;		/* increment iss */
	SPX_UNLOCK();
}

/*
 * SPX timer processing.
 */
static void
spx_timers(struct spxpcb *cb, int timer)
{
	long rexmt;
	int win;

	IPX_LIST_LOCK_ASSERT();
	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	cb->s_force = 1 + timer;
	switch (timer) {
	case SPXT_2MSL:
		/*
		 * 2 MSL timeout in shutdown went off.  TCP deletes
		 * connection control block.
		 */
		printf("spx: SPXT_2MSL went off for no reason\n");
		cb->s_timer[timer] = 0;
		break;

	case SPXT_REXMT:
		/*
		 * Retransmission timer went off.  Message has not been acked
		 * within retransmit interval.  Back off to a longer
		 * retransmit interval and retransmit one packet.
		 */
		if (++cb->s_rxtshift > SPX_MAXRXTSHIFT) {
			cb->s_rxtshift = SPX_MAXRXTSHIFT;
			spxstat.spxs_timeoutdrop++;
			spx_drop(cb, ETIMEDOUT);
			break;
		}
		spxstat.spxs_rexmttimeo++;
		rexmt = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;
		rexmt *= spx_backoff[cb->s_rxtshift];
		SPXT_RANGESET(cb->s_rxtcur, rexmt, SPXTV_MIN, SPXTV_REXMTMAX);
		cb->s_timer[SPXT_REXMT] = cb->s_rxtcur;

		/*
		 * If we have backed off fairly far, our srtt estimate is
		 * probably bogus.  Clobber it so we'll take the next rtt
		 * measurement as our srtt; move the current srtt into rttvar
		 * to keep the current retransmit times until then.
		 */
		if (cb->s_rxtshift > SPX_MAXRXTSHIFT / 4 ) {
			cb->s_rttvar += (cb->s_srtt >> 2);
			cb->s_srtt = 0;
		}
		cb->s_snxt = cb->s_rack;

		/*
		 * If timing a packet, stop the timer.
		 */
		cb->s_rtt = 0;

		/*
		 * See very long discussion in tcp_timer.c about congestion
		 * window and sstrhesh.
		 */
		win = min(cb->s_swnd, (cb->s_cwnd/CUNIT)) / 2;
		if (win < 2)
			win = 2;
		cb->s_cwnd = CUNIT;
		cb->s_ssthresh = win * CUNIT;
		spx_output(cb, NULL);
		break;

	case SPXT_PERSIST:
		/*
		 * Persistance timer into zero window.  Force a probe to be
		 * sent.
		 */
		spxstat.spxs_persisttimeo++;
		spx_setpersist(cb);
		spx_output(cb, NULL);
		break;

	case SPXT_KEEP:
		/*
		 * Keep-alive timer went off; send something or drop
		 * connection if idle for too long.
		 */
		spxstat.spxs_keeptimeo++;
		if (cb->s_state < TCPS_ESTABLISHED)
			goto dropit;
		if (cb->s_ipxpcb->ipxp_socket->so_options & SO_KEEPALIVE) {
		    	if (cb->s_idle >= SPXTV_MAXIDLE)
				goto dropit;
			spxstat.spxs_keepprobe++;
			spx_output(cb, NULL);
		} else
			cb->s_idle = 0;
		cb->s_timer[SPXT_KEEP] = SPXTV_KEEP;
		break;

	dropit:
		spxstat.spxs_keepdrops++;
		spx_drop(cb, ETIMEDOUT);
		break;

	default:
		panic("spx_timers: unknown timer %d", timer);
	}
}
