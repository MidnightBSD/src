/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tcp_timer.c	8.2 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/cc.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/ip_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif

int	tcp_keepinit;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINIT, keepinit, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepinit, 0, sysctl_msec_to_ticks, "I", "time to establish connection");

int	tcp_keepidle;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPIDLE, keepidle, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepidle, 0, sysctl_msec_to_ticks, "I", "time before keepalive probes begin");

int	tcp_keepintvl;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINTVL, keepintvl, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepintvl, 0, sysctl_msec_to_ticks, "I", "time between keepalive probes");

int	tcp_delacktime;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_DELACKTIME, delacktime, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_delacktime, 0, sysctl_msec_to_ticks, "I",
    "Time before a delayed ACK is sent");

int	tcp_msl;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, msl, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_msl, 0, sysctl_msec_to_ticks, "I", "Maximum segment lifetime");

int	tcp_rexmit_min;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_min, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_rexmit_min, 0, sysctl_msec_to_ticks, "I",
    "Minimum Retransmission Timeout");

int	tcp_rexmit_slop;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_slop, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_rexmit_slop, 0, sysctl_msec_to_ticks, "I",
    "Retransmission Timer Slop");

static int	always_keepalive = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, always_keepalive, CTLFLAG_RW,
    &always_keepalive , 0, "Assume SO_KEEPALIVE on all TCP connections");

int    tcp_fast_finwait2_recycle = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, fast_finwait2_recycle, CTLFLAG_RW, 
    &tcp_fast_finwait2_recycle, 0,
    "Recycle closed FIN_WAIT_2 connections faster");

int    tcp_finwait2_timeout;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, finwait2_timeout, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_finwait2_timeout, 0, sysctl_msec_to_ticks, "I", "FIN-WAIT2 timeout");

int	tcp_keepcnt = TCPTV_KEEPCNT;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, keepcnt, CTLFLAG_RW, &tcp_keepcnt, 0,
    "Number of keepalive probes to send");

	/* max idle probes */
int	tcp_maxpersistidle;

static int	per_cpu_timers = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, per_cpu_timers, CTLFLAG_RW,
    &per_cpu_timers , 0, "run tcp timers on all cpus");

#define	INP_CPU(inp)	(per_cpu_timers ? (!CPU_ABSENT(((inp)->inp_flowid % (mp_maxid+1))) ? \
		((inp)->inp_flowid % (mp_maxid+1)) : curcpu) : 0)

/*
 * Tcp protocol timeout routine called every 500 ms.
 * Updates timestamps used for TCP
 * causes finite state machine actions if timers expire.
 */
void
tcp_slowtimo(void)
{
	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		INP_INFO_WLOCK(&V_tcbinfo);
		(void) tcp_tw_2msl_scan(0);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
}

int	tcp_syn_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 1, 1, 1, 1, 2, 4, 8, 16, 32, 64, 64, 64 };

int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 512, 512, 512 };

static int tcp_totbackoff = 2559;	/* sum of tcp_backoff[] */

static int tcp_timer_race;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, timer_race, CTLFLAG_RD, &tcp_timer_race,
    0, "Count of t_inpcb races on tcp_discardcb");

/*
 * TCP timer processing.
 */

void
tcp_timer_delack(void *xtp)
{
	struct tcpcb *tp = xtp;
	struct inpcb *inp;
	CURVNET_SET(tp->t_vnet);

	inp = tp->t_inpcb;
	/*
	 * XXXRW: While this assert is in fact correct, bugs in the tcpcb
	 * tear-down mean we need it as a work-around for races between
	 * timers and tcp_discardcb().
	 *
	 * KASSERT(inp != NULL, ("tcp_timer_delack: inp == NULL"));
	 */
	if (inp == NULL) {
		tcp_timer_race++;
		CURVNET_RESTORE();
		return;
	}
	INP_WLOCK(inp);
	if ((inp->inp_flags & INP_DROPPED) || callout_pending(&tp->t_timers->tt_delack)
	    || !callout_active(&tp->t_timers->tt_delack)) {
		INP_WUNLOCK(inp);
		CURVNET_RESTORE();
		return;
	}
	callout_deactivate(&tp->t_timers->tt_delack);

	tp->t_flags |= TF_ACKNOW;
	TCPSTAT_INC(tcps_delack);
	(void) tcp_output(tp);
	INP_WUNLOCK(inp);
	CURVNET_RESTORE();
}

void
tcp_timer_2msl(void *xtp)
{
	struct tcpcb *tp = xtp;
	struct inpcb *inp;
	CURVNET_SET(tp->t_vnet);
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	/*
	 * XXXRW: Does this actually happen?
	 */
	INP_INFO_WLOCK(&V_tcbinfo);
	inp = tp->t_inpcb;
	/*
	 * XXXRW: While this assert is in fact correct, bugs in the tcpcb
	 * tear-down mean we need it as a work-around for races between
	 * timers and tcp_discardcb().
	 *
	 * KASSERT(inp != NULL, ("tcp_timer_2msl: inp == NULL"));
	 */
	if (inp == NULL) {
		tcp_timer_race++;
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	INP_WLOCK(inp);
	tcp_free_sackholes(tp);
	if ((inp->inp_flags & INP_DROPPED) || callout_pending(&tp->t_timers->tt_2msl) ||
	    !callout_active(&tp->t_timers->tt_2msl)) {
		INP_WUNLOCK(tp->t_inpcb);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	callout_deactivate(&tp->t_timers->tt_2msl);
	/*
	 * 2 MSL timeout in shutdown went off.  If we're closed but
	 * still waiting for peer to close and connection has been idle
	 * too long, or if 2MSL time is up from TIME_WAIT, delete connection
	 * control block.  Otherwise, check again in a bit.
	 *
	 * If fastrecycle of FIN_WAIT_2, in FIN_WAIT_2 and receiver has closed, 
	 * there's no point in hanging onto FIN_WAIT_2 socket. Just close it. 
	 * Ignore fact that there were recent incoming segments.
	 */
	if (tcp_fast_finwait2_recycle && tp->t_state == TCPS_FIN_WAIT_2 &&
	    tp->t_inpcb && tp->t_inpcb->inp_socket && 
	    (tp->t_inpcb->inp_socket->so_rcv.sb_state & SBS_CANTRCVMORE)) {
		TCPSTAT_INC(tcps_finwait2_drops);
		tp = tcp_close(tp);             
	} else {
		if (tp->t_state != TCPS_TIME_WAIT &&
		   ticks - tp->t_rcvtime <= TP_MAXIDLE(tp))
		       callout_reset_on(&tp->t_timers->tt_2msl,
			   TP_KEEPINTVL(tp), tcp_timer_2msl, tp, INP_CPU(inp));
	       else
		       tp = tcp_close(tp);
       }

#ifdef TCPDEBUG
	if (tp != NULL && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	if (tp != NULL)
		INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	CURVNET_RESTORE();
}

void
tcp_timer_keep(void *xtp)
{
	struct tcpcb *tp = xtp;
	struct tcptemp *t_template;
	struct inpcb *inp;
	CURVNET_SET(tp->t_vnet);
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	INP_INFO_WLOCK(&V_tcbinfo);
	inp = tp->t_inpcb;
	/*
	 * XXXRW: While this assert is in fact correct, bugs in the tcpcb
	 * tear-down mean we need it as a work-around for races between
	 * timers and tcp_discardcb().
	 *
	 * KASSERT(inp != NULL, ("tcp_timer_keep: inp == NULL"));
	 */
	if (inp == NULL) {
		tcp_timer_race++;
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	INP_WLOCK(inp);
	if ((inp->inp_flags & INP_DROPPED) || callout_pending(&tp->t_timers->tt_keep)
	    || !callout_active(&tp->t_timers->tt_keep)) {
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	callout_deactivate(&tp->t_timers->tt_keep);
	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
	TCPSTAT_INC(tcps_keeptimeo);
	if (tp->t_state < TCPS_ESTABLISHED)
		goto dropit;
	if ((always_keepalive || inp->inp_socket->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if (ticks - tp->t_rcvtime >= TP_KEEPIDLE(tp) + TP_MAXIDLE(tp))
			goto dropit;
		/*
		 * Send a packet designed to force a response
		 * if the peer is up and reachable:
		 * either an ACK if the connection is still alive,
		 * or an RST if the peer has closed the connection
		 * due to timeout or reboot.
		 * Using sequence number tp->snd_una-1
		 * causes the transmitted zero-length segment
		 * to lie outside the receive window;
		 * by the protocol spec, this requires the
		 * correspondent TCP to respond.
		 */
		TCPSTAT_INC(tcps_keepprobe);
		t_template = tcpip_maketemplate(inp);
		if (t_template) {
			tcp_respond(tp, t_template->tt_ipgen,
				    &t_template->tt_t, (struct mbuf *)NULL,
				    tp->rcv_nxt, tp->snd_una - 1, 0);
			free(t_template, M_TEMP);
		}
		callout_reset_on(&tp->t_timers->tt_keep, TP_KEEPINTVL(tp),
		    tcp_timer_keep, tp, INP_CPU(inp));
	} else
		callout_reset_on(&tp->t_timers->tt_keep, TP_KEEPIDLE(tp),
		    tcp_timer_keep, tp, INP_CPU(inp));

#ifdef TCPDEBUG
	if (inp->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	CURVNET_RESTORE();
	return;

dropit:
	TCPSTAT_INC(tcps_keepdrops);
	tp = tcp_drop(tp, ETIMEDOUT);

#ifdef TCPDEBUG
	if (tp != NULL && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	if (tp != NULL)
		INP_WUNLOCK(tp->t_inpcb);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	CURVNET_RESTORE();
}

void
tcp_timer_persist(void *xtp)
{
	struct tcpcb *tp = xtp;
	struct inpcb *inp;
	CURVNET_SET(tp->t_vnet);
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	INP_INFO_WLOCK(&V_tcbinfo);
	inp = tp->t_inpcb;
	/*
	 * XXXRW: While this assert is in fact correct, bugs in the tcpcb
	 * tear-down mean we need it as a work-around for races between
	 * timers and tcp_discardcb().
	 *
	 * KASSERT(inp != NULL, ("tcp_timer_persist: inp == NULL"));
	 */
	if (inp == NULL) {
		tcp_timer_race++;
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	INP_WLOCK(inp);
	if ((inp->inp_flags & INP_DROPPED) || callout_pending(&tp->t_timers->tt_persist)
	    || !callout_active(&tp->t_timers->tt_persist)) {
		INP_WUNLOCK(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	callout_deactivate(&tp->t_timers->tt_persist);
	/*
	 * Persistance timer into zero window.
	 * Force a byte to be output, if possible.
	 */
	TCPSTAT_INC(tcps_persisttimeo);
	/*
	 * Hack: if the peer is dead/unreachable, we do not
	 * time out if the window is closed.  After a full
	 * backoff, drop the connection if the idle time
	 * (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 */
	if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
	    (ticks - tp->t_rcvtime >= tcp_maxpersistidle ||
	     ticks - tp->t_rcvtime >= TCP_REXMTVAL(tp) * tcp_totbackoff)) {
		TCPSTAT_INC(tcps_persistdrop);
		tp = tcp_drop(tp, ETIMEDOUT);
		goto out;
	}
	tcp_setpersist(tp);
	tp->t_flags |= TF_FORCEDATA;
	(void) tcp_output(tp);
	tp->t_flags &= ~TF_FORCEDATA;

out:
#ifdef TCPDEBUG
	if (tp != NULL && tp->t_inpcb->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	if (tp != NULL)
		INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	CURVNET_RESTORE();
}

void
tcp_timer_rexmt(void * xtp)
{
	struct tcpcb *tp = xtp;
	CURVNET_SET(tp->t_vnet);
	int rexmt;
	int headlocked;
	struct inpcb *inp;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	INP_INFO_RLOCK(&V_tcbinfo);
	inp = tp->t_inpcb;
	/*
	 * XXXRW: While this assert is in fact correct, bugs in the tcpcb
	 * tear-down mean we need it as a work-around for races between
	 * timers and tcp_discardcb().
	 *
	 * KASSERT(inp != NULL, ("tcp_timer_rexmt: inp == NULL"));
	 */
	if (inp == NULL) {
		tcp_timer_race++;
		INP_INFO_RUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	INP_WLOCK(inp);
	if ((inp->inp_flags & INP_DROPPED) || callout_pending(&tp->t_timers->tt_rexmt)
	    || !callout_active(&tp->t_timers->tt_rexmt)) {
		INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
		return;
	}
	callout_deactivate(&tp->t_timers->tt_rexmt);
	tcp_free_sackholes(tp);
	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 */
	if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
		tp->t_rxtshift = TCP_MAXRXTSHIFT;
		TCPSTAT_INC(tcps_timeoutdrop);
		in_pcbref(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);
		INP_WUNLOCK(inp);
		INP_INFO_WLOCK(&V_tcbinfo);
		INP_WLOCK(inp);
		if (in_pcbrele_wlocked(inp)) {
			INP_INFO_WUNLOCK(&V_tcbinfo);
			CURVNET_RESTORE();
			return;
		}
		if (inp->inp_flags & INP_DROPPED) {
			INP_WUNLOCK(inp);
			INP_INFO_WUNLOCK(&V_tcbinfo);
			CURVNET_RESTORE();
			return;
		}

		tp = tcp_drop(tp, tp->t_softerror ?
			      tp->t_softerror : ETIMEDOUT);
		headlocked = 1;
		goto out;
	}
	INP_INFO_RUNLOCK(&V_tcbinfo);
	headlocked = 0;
	if (tp->t_rxtshift == 1) {
		/*
		 * first retransmit; record ssthresh and cwnd so they can
		 * be recovered if this turns out to be a "bad" retransmit.
		 * A retransmit is considered "bad" if an ACK for this
		 * segment is received within RTT/2 interval; the assumption
		 * here is that the ACK was already in flight.  See
		 * "On Estimating End-to-End Network Path Properties" by
		 * Allman and Paxson for more details.
		 */
		tp->snd_cwnd_prev = tp->snd_cwnd;
		tp->snd_ssthresh_prev = tp->snd_ssthresh;
		tp->snd_recover_prev = tp->snd_recover;
		if (IN_FASTRECOVERY(tp->t_flags))
			tp->t_flags |= TF_WASFRECOVERY;
		else
			tp->t_flags &= ~TF_WASFRECOVERY;
		if (IN_CONGRECOVERY(tp->t_flags))
			tp->t_flags |= TF_WASCRECOVERY;
		else
			tp->t_flags &= ~TF_WASCRECOVERY;
		tp->t_badrxtwin = ticks + (tp->t_srtt >> (TCP_RTT_SHIFT + 1));
		tp->t_flags |= TF_PREVVALID;
	} else
		tp->t_flags &= ~TF_PREVVALID;
	TCPSTAT_INC(tcps_rexmttimeo);
	if (tp->t_state == TCPS_SYN_SENT)
		rexmt = TCP_REXMTVAL(tp) * tcp_syn_backoff[tp->t_rxtshift];
	else
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
	TCPT_RANGESET(tp->t_rxtcur, rexmt,
		      tp->t_rttmin, TCPTV_REXMTMAX);
	/*
	 * Disable rfc1323 if we haven't got any response to
	 * our third SYN to work-around some broken terminal servers
	 * (most of which have hopefully been retired) that have bad VJ
	 * header compression code which trashes TCP segments containing
	 * unknown-to-them TCP options.
	 */
	if ((tp->t_state == TCPS_SYN_SENT) && (tp->t_rxtshift == 3))
		tp->t_flags &= ~(TF_REQ_SCALE|TF_REQ_TSTMP);
	/*
	 * If we backed off this far, our srtt estimate is probably bogus.
	 * Clobber it so we'll take the next rtt measurement as our srtt;
	 * move the current srtt into rttvar to keep the current
	 * retransmit times until then.
	 */
	if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
#ifdef INET6
		if ((tp->t_inpcb->inp_vflag & INP_IPV6) != 0)
			in6_losing(tp->t_inpcb);
		else
#endif
		tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
		tp->t_srtt = 0;
	}
	tp->snd_nxt = tp->snd_una;
	tp->snd_recover = tp->snd_max;
	/*
	 * Force a segment to be sent.
	 */
	tp->t_flags |= TF_ACKNOW;
	/*
	 * If timing a segment in this window, stop the timer.
	 */
	tp->t_rtttime = 0;

	cc_cong_signal(tp, NULL, CC_RTO);

	(void) tcp_output(tp);

out:
#ifdef TCPDEBUG
	if (tp != NULL && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	if (tp != NULL)
		INP_WUNLOCK(inp);
	if (headlocked)
		INP_INFO_WUNLOCK(&V_tcbinfo);
	CURVNET_RESTORE();
}

void
tcp_timer_activate(struct tcpcb *tp, int timer_type, u_int delta)
{
	struct callout *t_callout;
	void *f_callout;
	struct inpcb *inp = tp->t_inpcb;
	int cpu = INP_CPU(inp);

	switch (timer_type) {
		case TT_DELACK:
			t_callout = &tp->t_timers->tt_delack;
			f_callout = tcp_timer_delack;
			break;
		case TT_REXMT:
			t_callout = &tp->t_timers->tt_rexmt;
			f_callout = tcp_timer_rexmt;
			break;
		case TT_PERSIST:
			t_callout = &tp->t_timers->tt_persist;
			f_callout = tcp_timer_persist;
			break;
		case TT_KEEP:
			t_callout = &tp->t_timers->tt_keep;
			f_callout = tcp_timer_keep;
			break;
		case TT_2MSL:
			t_callout = &tp->t_timers->tt_2msl;
			f_callout = tcp_timer_2msl;
			break;
		default:
			panic("bad timer_type");
		}
	if (delta == 0) {
		callout_stop(t_callout);
	} else {
		callout_reset_on(t_callout, delta, f_callout, tp, cpu);
	}
}

int
tcp_timer_active(struct tcpcb *tp, int timer_type)
{
	struct callout *t_callout;

	switch (timer_type) {
		case TT_DELACK:
			t_callout = &tp->t_timers->tt_delack;
			break;
		case TT_REXMT:
			t_callout = &tp->t_timers->tt_rexmt;
			break;
		case TT_PERSIST:
			t_callout = &tp->t_timers->tt_persist;
			break;
		case TT_KEEP:
			t_callout = &tp->t_timers->tt_keep;
			break;
		case TT_2MSL:
			t_callout = &tp->t_timers->tt_2msl;
			break;
		default:
			panic("bad timer_type");
		}
	return callout_active(t_callout);
}

#define	ticks_to_msecs(t)	(1000*(t) / hz)

void
tcp_timer_to_xtimer(struct tcpcb *tp, struct tcp_timer *timer, struct xtcp_timer *xtimer)
{
	bzero(xtimer, sizeof(struct xtcp_timer));
	if (timer == NULL)
		return;
	if (callout_active(&timer->tt_delack))
		xtimer->tt_delack = ticks_to_msecs(timer->tt_delack.c_time - ticks);
	if (callout_active(&timer->tt_rexmt))
		xtimer->tt_rexmt = ticks_to_msecs(timer->tt_rexmt.c_time - ticks);
	if (callout_active(&timer->tt_persist))
		xtimer->tt_persist = ticks_to_msecs(timer->tt_persist.c_time - ticks);
	if (callout_active(&timer->tt_keep))
		xtimer->tt_keep = ticks_to_msecs(timer->tt_keep.c_time - ticks);
	if (callout_active(&timer->tt_2msl))
		xtimer->tt_2msl = ticks_to_msecs(timer->tt_2msl.c_time - ticks);
	xtimer->t_rcvtime = ticks_to_msecs(ticks - tp->t_rcvtime);
}
