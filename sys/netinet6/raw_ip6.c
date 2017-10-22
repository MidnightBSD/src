/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/sys/netinet6/raw_ip6.c 171260 2007-07-05 16:29:40Z delphij $
 */

/*-
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */

#include "opt_ipsec.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/icmp6.h>
#include <netinet/in_pcb.h>
#include <netinet/ip6.h>
#include <netinet6/ip6protosw.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/raw_ip6.h>
#include <netinet6/scope6_var.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif /* IPSEC */

#include <machine/stdarg.h>

#define	satosin6(sa)	((struct sockaddr_in6 *)(sa))
#define	ifatoia6(ifa)	((struct in6_ifaddr *)(ifa))

/*
 * Raw interface to IP6 protocol.
 */

extern struct	inpcbhead ripcb;
extern struct	inpcbinfo ripcbinfo;
extern u_long	rip_sendspace;
extern u_long	rip_recvspace;

struct rip6stat rip6stat;

/*
 * Hooks for multicast forwarding.
 */
struct socket *ip6_mrouter = NULL;
int (*ip6_mrouter_set)(struct socket *, struct sockopt *);
int (*ip6_mrouter_get)(struct socket *, struct sockopt *);
int (*ip6_mrouter_done)(void);
int (*ip6_mforward)(struct ip6_hdr *, struct ifnet *, struct mbuf *);
int (*mrt6_ioctl)(int, caddr_t);

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
int
rip6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	register struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	register struct inpcb *in6p;
	struct inpcb *last = 0;
	struct mbuf *opts = NULL;
	struct sockaddr_in6 fromsa;

	rip6stat.rip6s_ipackets++;

	if (faithprefix_p != NULL && (*faithprefix_p)(&ip6->ip6_dst)) {
		/* XXX send icmp6 host/port unreach? */
		m_freem(m);
		return IPPROTO_DONE;
	}

	init_sin6(&fromsa, m); /* general init */

	INP_INFO_RLOCK(&ripcbinfo);
	LIST_FOREACH(in6p, &ripcb, inp_list) {
		INP_LOCK(in6p);
		if ((in6p->in6p_vflag & INP_IPV6) == 0) {
docontinue:
			INP_UNLOCK(in6p);
			continue;
		}
		if (in6p->in6p_ip6_nxt &&
		    in6p->in6p_ip6_nxt != proto)
			goto docontinue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) &&
		    !IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &ip6->ip6_dst))
			goto docontinue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) &&
		    !IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, &ip6->ip6_src))
			goto docontinue;
		if (in6p->in6p_cksum != -1) {
			rip6stat.rip6s_isum++;
			if (in6_cksum(m, proto, *offp,
			    m->m_pkthdr.len - *offp)) {
				rip6stat.rip6s_badsum++;
				goto docontinue;
			}
		}
		if (last) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

#ifdef IPSEC
			/*
			 * Check AH/ESP integrity.
			 */
			if (n && ipsec6_in_reject(n, last)) {
				m_freem(n);
				ipsec6stat.in_polvio++;
				/* do not inject data into pcb */
			} else
#endif /* IPSEC */
			if (n) {
				if (last->in6p_flags & IN6P_CONTROLOPTS ||
				    last->in6p_socket->so_options & SO_TIMESTAMP)
					ip6_savecontrol(last, n, &opts);
				/* strip intermediate headers */
				m_adj(n, *offp);
				if (sbappendaddr(&last->in6p_socket->so_rcv,
						(struct sockaddr *)&fromsa,
						 n, opts) == 0) {
					m_freem(n);
					if (opts)
						m_freem(opts);
					rip6stat.rip6s_fullsock++;
				} else
					sorwakeup(last->in6p_socket);
				opts = NULL;
			}
			INP_UNLOCK(last);
		}
		last = in6p;
	}
#ifdef IPSEC
	/*
	 * Check AH/ESP integrity.
	 */
	if (last && ipsec6_in_reject(m, last)) {
		m_freem(m);
		ipsec6stat.in_polvio++;
		ip6stat.ip6s_delivered--;
		/* do not inject data into pcb */
		INP_UNLOCK(last);
	} else
#endif /* IPSEC */
	if (last) {
		if (last->in6p_flags & IN6P_CONTROLOPTS ||
		    last->in6p_socket->so_options & SO_TIMESTAMP)
			ip6_savecontrol(last, m, &opts);
		/* strip intermediate headers */
		m_adj(m, *offp);
		if (sbappendaddr(&last->in6p_socket->so_rcv,
				(struct sockaddr *)&fromsa, m, opts) == 0) {
			m_freem(m);
			if (opts)
				m_freem(opts);
			rip6stat.rip6s_fullsock++;
		} else
			sorwakeup(last->in6p_socket);
		INP_UNLOCK(last);
	} else {
		rip6stat.rip6s_nosock++;
		if (m->m_flags & M_MCAST)
			rip6stat.rip6s_nosockmcast++;
		if (proto == IPPROTO_NONE)
			m_freem(m);
		else {
			char *prvnxtp = ip6_get_prevhdr(m, *offp); /* XXX */
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_NEXTHEADER,
				    prvnxtp - mtod(m, char *));
		}
		ip6stat.ip6s_delivered--;
	}
	INP_INFO_RUNLOCK(&ripcbinfo);
	return IPPROTO_DONE;
}

void
rip6_ctlinput(int cmd, struct sockaddr *sa, void *d)
{
	struct ip6_hdr *ip6;
	struct mbuf *m;
	int off = 0;
	struct ip6ctlparam *ip6cp = NULL;
	const struct sockaddr_in6 *sa6_src = NULL;
	void *cmdarg;
	struct inpcb *(*notify) __P((struct inpcb *, int)) = in6_rtchange;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (PRC_IS_REDIRECT(cmd))
		notify = in6_rtchange, d = NULL;
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (inet6ctlerrmap[cmd] == 0)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		cmdarg = ip6cp->ip6c_cmdarg;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		cmdarg = NULL;
		sa6_src = &sa6_any;
	}

	(void) in6_pcbnotify(&ripcbinfo, sa, 0,
			     (const struct sockaddr *)sa6_src,
			     0, cmd, cmdarg, notify);
}

/*
 * Generate IPv6 header and pass packet to ip6_output.
 * Tack on options user may have setup with control call.
 */
int
#if __STDC__
rip6_output(struct mbuf *m, ...)
#else
rip6_output(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	struct mbuf *control;
	struct socket *so;
	struct sockaddr_in6 *dstsock;
	struct in6_addr *dst;
	struct ip6_hdr *ip6;
	struct inpcb *in6p;
	u_int	plen = m->m_pkthdr.len;
	int error = 0;
	struct ip6_pktopts opt, *optp;
	struct ifnet *oifp = NULL;
	int type = 0, code = 0;		/* for ICMPv6 output statistics only */
	int priv = 0;
	int scope_ambiguous = 0;
	struct in6_addr *in6a;
	va_list ap;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	dstsock = va_arg(ap, struct sockaddr_in6 *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

	in6p = sotoin6pcb(so);
	INP_LOCK(in6p);

	priv = 0;
	if (suser_cred(so->so_cred, 0) == 0)
		priv = 1;
	dst = &dstsock->sin6_addr;
	if (control) {
		if ((error = ip6_setpktopts(control, &opt,
		    in6p->in6p_outputopts, priv, so->so_proto->pr_protocol))
		    != 0) {
			goto bad;
		}
		optp = &opt;
	} else
		optp = in6p->in6p_outputopts;

	/*
	 * Check and convert scope zone ID into internal form.
	 * XXX: we may still need to determine the zone later.
	 */
	if (!(so->so_state & SS_ISCONNECTED)) {
		if (dstsock->sin6_scope_id == 0 && !ip6_use_defzone)
			scope_ambiguous = 1;
		if ((error = sa6_embedscope(dstsock, ip6_use_defzone)) != 0)
			goto bad;
	}

	/*
	 * For an ICMPv6 packet, we should know its type and code
	 * to update statistics.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icmp6;
		if (m->m_len < sizeof(struct icmp6_hdr) &&
		    (m = m_pullup(m, sizeof(struct icmp6_hdr))) == NULL) {
			error = ENOBUFS;
			goto bad;
		}
		icmp6 = mtod(m, struct icmp6_hdr *);
		type = icmp6->icmp6_type;
		code = icmp6->icmp6_code;
	}

	M_PREPEND(m, sizeof(*ip6), M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto bad;
	}
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Source address selection.
	 */
	if ((in6a = in6_selectsrc(dstsock, optp, in6p->in6p_moptions, NULL,
	    &in6p->in6p_laddr, &oifp, &error)) == NULL) {
		if (error == 0)
			error = EADDRNOTAVAIL;
		goto bad;
	}
	ip6->ip6_src = *in6a;

	if (oifp && scope_ambiguous) {
		/*
		 * Application should provide a proper zone ID or the use of
		 * default zone IDs should be enabled.  Unfortunately, some
		 * applications do not behave as it should, so we need a
		 * workaround.  Even if an appropriate ID is not determined
		 * (when it's required), if we can determine the outgoing
		 * interface. determine the zone ID based on the interface.
		 */
		error = in6_setscope(&dstsock->sin6_addr, oifp, NULL);
		if (error != 0)
			goto bad;
	}
	ip6->ip6_dst = dstsock->sin6_addr;

	/* fill in the rest of the IPv6 header fields */
	ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
		(in6p->in6p_flowinfo & IPV6_FLOWINFO_MASK);
	ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
		(IPV6_VERSION & IPV6_VERSION_MASK);
	/* ip6_plen will be filled in ip6_output, so not fill it here. */
	ip6->ip6_nxt = in6p->in6p_ip6_nxt;
	ip6->ip6_hlim = in6_selecthlim(in6p, oifp);

	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6 ||
	    in6p->in6p_cksum != -1) {
		struct mbuf *n;
		int off;
		u_int16_t *p;

		/* compute checksum */
		if (so->so_proto->pr_protocol == IPPROTO_ICMPV6)
			off = offsetof(struct icmp6_hdr, icmp6_cksum);
		else
			off = in6p->in6p_cksum;
		if (plen < off + 1) {
			error = EINVAL;
			goto bad;
		}
		off += sizeof(struct ip6_hdr);

		n = m;
		while (n && n->m_len <= off) {
			off -= n->m_len;
			n = n->m_next;
		}
		if (!n)
			goto bad;
		p = (u_int16_t *)(mtod(n, caddr_t) + off);
		*p = 0;
		*p = in6_cksum(m, ip6->ip6_nxt, sizeof(*ip6), plen);
	}

	error = ip6_output(m, optp, NULL, 0, in6p->in6p_moptions, &oifp, in6p);
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		if (oifp)
			icmp6_ifoutstat_inc(oifp, type, code);
		icmp6stat.icp6s_outhist[type]++;
	} else
		rip6stat.rip6s_opackets++;

	goto freectl;

 bad:
	if (m)
		m_freem(m);

 freectl:
	if (control) {
		ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	INP_UNLOCK(in6p);
	return (error);
}

/*
 * Raw IPv6 socket option processing.
 */
int
rip6_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int error;

	if (sopt->sopt_level == IPPROTO_ICMPV6)
		/*
		 * XXX: is it better to call icmp6_ctloutput() directly
		 * from protosw?
		 */
		return (icmp6_ctloutput(so, sopt));
	else if (sopt->sopt_level != IPPROTO_IPV6)
		return (EINVAL);

	error = 0;

	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case MRT6_INIT:
		case MRT6_DONE:
		case MRT6_ADD_MIF:
		case MRT6_DEL_MIF:
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
		case MRT6_PIM:
			error = ip6_mrouter_get ?  ip6_mrouter_get(so, sopt) :
			    EOPNOTSUPP;
			break;
		case IPV6_CHECKSUM:
			error = ip6_raw_ctloutput(so, sopt);
			break;
		default:
			error = ip6_ctloutput(so, sopt);
			break;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case MRT6_INIT:
		case MRT6_DONE:
		case MRT6_ADD_MIF:
		case MRT6_DEL_MIF:
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
		case MRT6_PIM:
			error = ip6_mrouter_set ?  ip6_mrouter_set(so, sopt) :
			    EOPNOTSUPP;
			break;
		case IPV6_CHECKSUM:
			error = ip6_raw_ctloutput(so, sopt);
			break;
		default:
			error = ip6_ctloutput(so, sopt);
			break;
		}
		break;
	}

	return (error);
}

static int
rip6_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	struct icmp6_filter *filter;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("rip6_attach: inp != NULL"));
	if (td && (error = suser(td)) != 0)
		return error;
	error = soreserve(so, rip_sendspace, rip_recvspace);
	if (error)
		return error;
	MALLOC(filter, struct icmp6_filter *,
	       sizeof(struct icmp6_filter), M_PCB, M_NOWAIT);
	if (filter == NULL)
		return ENOMEM;
	INP_INFO_WLOCK(&ripcbinfo);
	error = in_pcballoc(so, &ripcbinfo);
	if (error) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		FREE(filter, M_PCB);
		return error;
	}
	inp = (struct inpcb *)so->so_pcb;
	INP_INFO_WUNLOCK(&ripcbinfo);
	inp->inp_vflag |= INP_IPV6;
	inp->in6p_ip6_nxt = (long)proto;
	inp->in6p_hops = -1;	/* use kernel default */
	inp->in6p_cksum = -1;
	inp->in6p_icmp6filt = filter;
	ICMP6_FILTER_SETPASSALL(inp->in6p_icmp6filt);
	INP_UNLOCK(inp);
	return 0;
}

static void
rip6_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_detach: inp == NULL"));

	if (so == ip6_mrouter && ip6_mrouter_done)
		ip6_mrouter_done();
	/* xxx: RSVP */
	INP_INFO_WLOCK(&ripcbinfo);
	INP_LOCK(inp);
	if (inp->in6p_icmp6filt) {
		FREE(inp->in6p_icmp6filt, M_PCB);
		inp->in6p_icmp6filt = NULL;
	}
	in6_pcbdetach(inp);
	in6_pcbfree(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
}

/* XXXRW: This can't ever be called. */
static void
rip6_abort(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_abort: inp == NULL"));

	soisdisconnected(so);
}

static void
rip6_close(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_close: inp == NULL"));

	soisdisconnected(so);
}

static int
rip6_disconnect(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);

	if ((so->so_state & SS_ISCONNECTED) == 0)
		return ENOTCONN;
	inp->in6p_faddr = in6addr_any;
	rip6_abort(so);
	return (0);
}

static int
rip6_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)nam;
	struct ifaddr *ia = NULL;
	int error = 0;

	KASSERT(inp != NULL, ("rip6_bind: inp == NULL"));
	if (nam->sa_len != sizeof(*addr))
		return EINVAL;
	if (TAILQ_EMPTY(&ifnet) || addr->sin6_family != AF_INET6)
		return EADDRNOTAVAIL;
	if ((error = sa6_embedscope(addr, ip6_use_defzone)) != 0)
		return(error);

	if (!IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr) &&
	    (ia = ifa_ifwithaddr((struct sockaddr *)addr)) == 0)
		return EADDRNOTAVAIL;
	if (ia &&
	    ((struct in6_ifaddr *)ia)->ia6_flags &
	    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|
	     IN6_IFF_DETACHED|IN6_IFF_DEPRECATED)) {
		return (EADDRNOTAVAIL);
	}
	INP_INFO_WLOCK(&ripcbinfo);
	INP_LOCK(inp);
	inp->in6p_laddr = addr->sin6_addr;
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return 0;
}

static int
rip6_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)nam;
	struct in6_addr *in6a = NULL;
	struct ifnet *ifp = NULL;
	int error = 0, scope_ambiguous = 0;

	KASSERT(inp != NULL, ("rip6_connect: inp == NULL"));
	if (nam->sa_len != sizeof(*addr))
		return EINVAL;
	if (TAILQ_EMPTY(&ifnet))
		return EADDRNOTAVAIL;
	if (addr->sin6_family != AF_INET6)
		return EAFNOSUPPORT;

	/*
	 * Application should provide a proper zone ID or the use of
	 * default zone IDs should be enabled.  Unfortunately, some
	 * applications do not behave as it should, so we need a
	 * workaround.  Even if an appropriate ID is not determined,
	 * we'll see if we can determine the outgoing interface.  If we
	 * can, determine the zone ID based on the interface below.
	 */
	if (addr->sin6_scope_id == 0 && !ip6_use_defzone)
		scope_ambiguous = 1;
	if ((error = sa6_embedscope(addr, ip6_use_defzone)) != 0)
		return(error);

	INP_INFO_WLOCK(&ripcbinfo);
	INP_LOCK(inp);
	/* Source address selection. XXX: need pcblookup? */
	in6a = in6_selectsrc(addr, inp->in6p_outputopts,
			     inp->in6p_moptions, NULL,
			     &inp->in6p_laddr, &ifp, &error);
	if (in6a == NULL) {
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&ripcbinfo);
		return (error ? error : EADDRNOTAVAIL);
	}

	/* XXX: see above */
	if (ifp && scope_ambiguous &&
	    (error = in6_setscope(&addr->sin6_addr, ifp, NULL)) != 0) {
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&ripcbinfo);
		return(error);
	}
	inp->in6p_faddr = addr->sin6_addr;
	inp->in6p_laddr = *in6a;
	soisconnected(so);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return 0;
}

static int
rip6_shutdown(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_shutdown: inp == NULL"));
	INP_LOCK(inp);
	socantsendmore(so);
	INP_UNLOCK(inp);
	return 0;
}

static int
rip6_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in6 tmp;
	struct sockaddr_in6 *dst;
	int ret;

	KASSERT(inp != NULL, ("rip6_send: inp == NULL"));
	INP_INFO_WLOCK(&ripcbinfo);
	/* always copy sockaddr to avoid overwrites */
	/* Unlocked read. */
	if (so->so_state & SS_ISCONNECTED) {
		if (nam) {
			INP_INFO_WUNLOCK(&ripcbinfo);
			m_freem(m);
			return EISCONN;
		}
		/* XXX */
		bzero(&tmp, sizeof(tmp));
		tmp.sin6_family = AF_INET6;
		tmp.sin6_len = sizeof(struct sockaddr_in6);
		bcopy(&inp->in6p_faddr, &tmp.sin6_addr,
		      sizeof(struct in6_addr));
		dst = &tmp;
	} else {
		if (nam == NULL) {
			INP_INFO_WUNLOCK(&ripcbinfo);
			m_freem(m);
			return ENOTCONN;
		}
		if (nam->sa_len != sizeof(struct sockaddr_in6)) {
			INP_INFO_WUNLOCK(&ripcbinfo);
			m_freem(m);
			return(EINVAL);
		}
		tmp = *(struct sockaddr_in6 *)nam;
		dst = &tmp;

		if (dst->sin6_family == AF_UNSPEC) {
			/*
			 * XXX: we allow this case for backward
			 * compatibility to buggy applications that
			 * rely on old (and wrong) kernel behavior.
			 */
			log(LOG_INFO, "rip6 SEND: address family is "
			    "unspec. Assume AF_INET6\n");
			dst->sin6_family = AF_INET6;
		} else if (dst->sin6_family != AF_INET6) {
			INP_INFO_WUNLOCK(&ripcbinfo);
			m_freem(m);
			return(EAFNOSUPPORT);
		}
	}
	ret = rip6_output(m, so, dst, control);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return (ret);
}

struct pr_usrreqs rip6_usrreqs = {
	.pru_abort =		rip6_abort,
	.pru_attach =		rip6_attach,
	.pru_bind =		rip6_bind,
	.pru_connect =		rip6_connect,
	.pru_control =		in6_control,
	.pru_detach =		rip6_detach,
	.pru_disconnect =	rip6_disconnect,
	.pru_peeraddr =		in6_getpeeraddr,
	.pru_send =		rip6_send,
	.pru_shutdown =		rip6_shutdown,
	.pru_sockaddr =		in6_getsockaddr,
	.pru_close =		rip6_close,
};
