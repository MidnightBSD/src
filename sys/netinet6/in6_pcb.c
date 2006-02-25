/*	$FreeBSD: src/sys/netinet6/in6_pcb.c,v 1.62.2.1 2005/11/04 20:26:15 ume Exp $	*/
/*	$KAME: in6_pcb.c,v 1.31 2001/05/21 05:45:10 jinmei Exp $	*/
  
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
 */

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/jail.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/tcp_var.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/scope6_var.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netinet6/ah.h>
#ifdef INET6
#include <netinet6/ah6.h>
#endif
#include <netkey/key.h>
#endif /* IPSEC */

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/key.h>
#endif /* FAST_IPSEC */

struct	in6_addr zeroin6_addr;

int
in6_pcbbind(inp, nam, cred)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct ucred *cred;
{
	struct socket *so = inp->inp_socket;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)NULL;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	u_short	lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);

	INP_INFO_WLOCK_ASSERT(pcbinfo);
	INP_LOCK_ASSERT(inp);

	if (!in6_ifaddr) /* XXX broken! */
		return (EADDRNOTAVAIL);
	if (inp->inp_lport || !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
		wild = 1;
	if (nam) {
		int error;

		sin6 = (struct sockaddr_in6 *)nam;
		if (nam->sa_len != sizeof(*sin6))
			return (EINVAL);
		/*
		 * family check.
		 */
		if (nam->sa_family != AF_INET6)
			return (EAFNOSUPPORT);

		if ((error = sa6_embedscope(sin6, ip6_use_defzone)) != 0)
			return(error);

		lport = sin6->sin6_port;
		if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow compepte duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			struct ifaddr *ia = NULL;

			sin6->sin6_port = 0;		/* yech... */
			if ((ia = ifa_ifwithaddr((struct sockaddr *)sin6)) == 0)
				return (EADDRNOTAVAIL);

			/*
			 * XXX: bind to an anycast address might accidentally
			 * cause sending a packet with anycast source address.
			 * We should allow to bind to a deprecated address, since
			 * the application dares to use it.
			 */
			if (ia &&
			    ((struct in6_ifaddr *)ia)->ia6_flags &
			    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|IN6_IFF_DETACHED)) {
				return (EADDRNOTAVAIL);
			}
		}
		if (lport) {
			struct inpcb *t;

			/* GROSS */
			if (ntohs(lport) < IPV6PORT_RESERVED &&
			    suser_cred(cred, SUSER_ALLOWJAIL))
				return (EACCES);
			if (so->so_cred->cr_uid != 0 &&
			    !IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
				t = in6_pcblookup_local(pcbinfo,
				    &sin6->sin6_addr, lport,
				    INPLOOKUP_WILDCARD);
				if (t &&
				    ((t->inp_vflag & INP_TIMEWAIT) == 0) &&
				    (so->so_type != SOCK_STREAM ||
				     IN6_IS_ADDR_UNSPECIFIED(&t->in6p_faddr)) &&
				    (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) ||
			    	     !IN6_IS_ADDR_UNSPECIFIED(&t->in6p_laddr) ||
				     (t->inp_socket->so_options & SO_REUSEPORT) 
				      == 0) && (so->so_cred->cr_uid !=
				     t->inp_socket->so_cred->cr_uid))
					return (EADDRINUSE);
				if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
				    IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
					struct sockaddr_in sin;

					in6_sin6_2_sin(&sin, sin6);
					t = in_pcblookup_local(pcbinfo,
						sin.sin_addr, lport,
						INPLOOKUP_WILDCARD);
					if (t &&
					    ((t->inp_vflag &
					      INP_TIMEWAIT) == 0) &&
					    (so->so_type != SOCK_STREAM ||
					     ntohl(t->inp_faddr.s_addr) ==
					      INADDR_ANY) &&
					    (so->so_cred->cr_uid !=
					     t->inp_socket->so_cred->cr_uid))
						return (EADDRINUSE);
				}
			}
			t = in6_pcblookup_local(pcbinfo, &sin6->sin6_addr,
						lport, wild);
			if (t && (reuseport & ((t->inp_vflag & INP_TIMEWAIT) ?
			    intotw(t)->tw_so_options : 
			    t->inp_socket->so_options)) == 0)
				return (EADDRINUSE);
			if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
			    IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
				struct sockaddr_in sin;

				in6_sin6_2_sin(&sin, sin6);
				t = in_pcblookup_local(pcbinfo, sin.sin_addr,
						       lport, wild);
				if (t && t->inp_vflag & INP_TIMEWAIT) {
					if ((reuseport & 
					    intotw(t)->tw_so_options) == 0 &&
					    (ntohl(t->inp_laddr.s_addr) !=
					     INADDR_ANY || ((inp->inp_vflag & 
					     INP_IPV6PROTO) == 
					     (t->inp_vflag & INP_IPV6PROTO))))
						return (EADDRINUSE);
				}
				else if (t && 
				    (reuseport & t->inp_socket->so_options) 
				    == 0 && (ntohl(t->inp_laddr.s_addr) != 
				    INADDR_ANY || INP_SOCKAF(so) ==
				     INP_SOCKAF(t->inp_socket)))
					return (EADDRINUSE);
			}
		}
		inp->in6p_laddr = sin6->sin6_addr;
	}
	if (lport == 0) {
		int e;
		if ((e = in6_pcbsetport(&inp->in6p_laddr, inp, cred)) != 0)
			return (e);
	}
	else {
		inp->inp_lport = lport;
		if (in_pcbinshash(inp) != 0) {
			inp->in6p_laddr = in6addr_any;
			inp->inp_lport = 0;
			return (EAGAIN);
		}
	}
	return (0);
}

/*
 *   Transform old in6_pcbconnect() into an inner subroutine for new
 *   in6_pcbconnect(): Do some validity-checking on the remote
 *   address (in mbuf 'nam') and then determine local host address
 *   (i.e., which interface) to use to access that remote host.
 *
 *   This preserves definition of in6_pcbconnect(), while supporting a
 *   slightly different version for T/TCP.  (This is more than
 *   a bit of a kludge, but cleaning up the internal interfaces would
 *   have forced minor changes in every protocol).
 */

int
in6_pcbladdr(inp, nam, plocal_addr6)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct in6_addr **plocal_addr6;
{
	register struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
	int error = 0;
	struct ifnet *ifp = NULL;
	int scope_ambiguous = 0;

	if (nam->sa_len != sizeof (*sin6))
		return (EINVAL);
	if (sin6->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);
	if (sin6->sin6_port == 0)
		return (EADDRNOTAVAIL);

	if (sin6->sin6_scope_id == 0 && !ip6_use_defzone)
		scope_ambiguous = 1;
	if ((error = sa6_embedscope(sin6, ip6_use_defzone)) != 0)
		return(error);

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	if (in6_ifaddr) {
		/*
		 * If the destination address is UNSPECIFIED addr,
		 * use the loopback addr, e.g ::1.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			sin6->sin6_addr = in6addr_loopback;
	}

	/*
	 * XXX: in6_selectsrc might replace the bound local address
	 * with the address specified by setsockopt(IPV6_PKTINFO).
	 * Is it the intended behavior?
	 */
	*plocal_addr6 = in6_selectsrc(sin6, inp->in6p_outputopts,
				      inp->in6p_moptions, NULL,
				      &inp->in6p_laddr, &ifp, &error);
	if (ifp && scope_ambiguous &&
	    (error = in6_setscope(&sin6->sin6_addr, ifp, NULL)) != 0) {
		return(error);
	}

	if (*plocal_addr6 == 0) {
		if (error == 0)
			error = EADDRNOTAVAIL;
		return (error);
	}
	/*
	 * Don't do pcblookup call here; return interface in
	 * plocal_addr6
	 * and exit to caller, that will do the lookup.
	 */

	return (0);
}

/*
 * Outer subroutine:
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(inp, nam, cred)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct ucred *cred;
{
	struct in6_addr *addr6;
	register struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
	int error;

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	/*
	 * Call inner routine, to assign local interface address.
	 * in6_pcbladdr() may automatically fill in sin6_scope_id.
	 */
	if ((error = in6_pcbladdr(inp, nam, &addr6)) != 0)
		return (error);

	if (in6_pcblookup_hash(inp->inp_pcbinfo, &sin6->sin6_addr,
			       sin6->sin6_port,
			      IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)
			      ? addr6 : &inp->in6p_laddr,
			      inp->inp_lport, 0, NULL) != NULL) {
		return (EADDRINUSE);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		if (inp->inp_lport == 0) {
			error = in6_pcbbind(inp, (struct sockaddr *)0, cred);
			if (error)
				return (error);
		}
		inp->in6p_laddr = *addr6;
	}
	inp->in6p_faddr = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	/* update flowinfo - draft-itojun-ipv6-flowlabel-api-00 */
	inp->in6p_flowinfo &= ~IPV6_FLOWLABEL_MASK;
	if (inp->in6p_flags & IN6P_AUTOFLOWLABEL)
		inp->in6p_flowinfo |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);

	in_pcbrehash(inp);
#ifdef IPSEC
	if (inp->inp_socket->so_type == SOCK_STREAM)
		ipsec_pcbconn(inp->inp_sp);
#endif
	return (0);
}

void
in6_pcbdisconnect(inp)
	struct inpcb *inp;
{

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	bzero((caddr_t)&inp->in6p_faddr, sizeof(inp->in6p_faddr));
	inp->inp_fport = 0;
	/* clear flowinfo - draft-itojun-ipv6-flowlabel-api-00 */
	inp->in6p_flowinfo &= ~IPV6_FLOWLABEL_MASK;
	in_pcbrehash(inp);
#ifdef IPSEC
	ipsec_pcbdisconn(inp->inp_sp);
#endif
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in6_pcbdetach(inp);
}

void
in6_pcbdetach(inp)
	struct inpcb *inp;
{
	struct socket *so = inp->inp_socket;
	struct inpcbinfo *ipi = inp->inp_pcbinfo;

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

#if defined(IPSEC) || defined(FAST_IPSEC)
	if (inp->in6p_sp != NULL)
		ipsec6_delete_pcbpolicy(inp);
#endif /* IPSEC */
	inp->inp_gencnt = ++ipi->ipi_gencnt;
	in_pcbremlists(inp);

	if (so) {
		ACCEPT_LOCK();
		SOCK_LOCK(so);
		so->so_pcb = NULL;
		sotryfree(so);
	}

 	ip6_freepcbopts(inp->in6p_outputopts);
 	ip6_freemoptions(inp->in6p_moptions);
	/* Check and free IPv4 related resources in case of mapped addr */
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
	ip_freemoptions(inp->inp_moptions);
	inp->inp_vflag = 0;
	INP_LOCK_DESTROY(inp);
	uma_zfree(ipi->ipi_zone, inp);
}

struct sockaddr *
in6_sockaddr(port, addr_p)
	in_port_t port;
	struct in6_addr *addr_p;
{
	struct sockaddr_in6 *sin6;

	MALLOC(sin6, struct sockaddr_in6 *, sizeof *sin6, M_SONAME, M_WAITOK);
	bzero(sin6, sizeof *sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_port = port;
	sin6->sin6_addr = *addr_p;
	(void)sa6_recoverscope(sin6); /* XXX: should catch errors */

	return (struct sockaddr *)sin6;
}

struct sockaddr *
in6_v4mapsin6_sockaddr(port, addr_p)
	in_port_t port;
	struct in_addr *addr_p;
{
	struct sockaddr_in sin;
	struct sockaddr_in6 *sin6_p;

	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = port;
	sin.sin_addr = *addr_p;

	MALLOC(sin6_p, struct sockaddr_in6 *, sizeof *sin6_p, M_SONAME,
		M_WAITOK);
	in6_sin_2_v4mapsin6(&sin, sin6_p);

	return (struct sockaddr *)sin6_p;
}

/*
 * The calling convention of in6_setsockaddr() and in6_setpeeraddr() was
 * modified to match the pru_sockaddr() and pru_peeraddr() entry points
 * in struct pr_usrreqs, so that protocols can just reference then directly
 * without the need for a wrapper function.  The socket must have a valid
 * (i.e., non-nil) PCB, but it should be impossible to get an invalid one
 * except through a kernel programming error, so it is acceptable to panic
 * (or in this case trap) if the PCB is invalid.  (Actually, we don't trap
 * because there actually /is/ a programming error somewhere... XXX)
 */
int
in6_setsockaddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	register struct inpcb *inp;
	struct in6_addr addr;
	in_port_t port;

	s = splnet();
	inp = sotoinpcb(so);
	if (!inp) {
		splx(s);
		return EINVAL;
	}
	port = inp->inp_lport;
	addr = inp->in6p_laddr;
	splx(s);

	*nam = in6_sockaddr(port, &addr);
	return 0;
}

int
in6_setpeeraddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	struct inpcb *inp;
	struct in6_addr addr;
	in_port_t port;

	s = splnet();
	inp = sotoinpcb(so);
	if (!inp) {
		splx(s);
		return EINVAL;
	}
	port = inp->inp_fport;
	addr = inp->in6p_faddr;
	splx(s);

	*nam = in6_sockaddr(port, &addr);
	return 0;
}

int
in6_mapped_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct	inpcb *inp = sotoinpcb(so);
	int	error;

	if (inp == NULL)
		return EINVAL;
	if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) == INP_IPV4) {
		error = in_setsockaddr(so, nam, &tcbinfo);
		if (error == 0)
			in6_sin_2_v4mapsin6_in_sock(nam);
	} else {
		/* scope issues will be handled in in6_setsockaddr(). */
		error = in6_setsockaddr(so, nam);
	}

	return error;
}

int
in6_mapped_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct	inpcb *inp = sotoinpcb(so);
	int	error;

	if (inp == NULL)
		return EINVAL;
	if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) == INP_IPV4) {
		error = in_setpeeraddr(so, nam, &tcbinfo);
		if (error == 0)
			in6_sin_2_v4mapsin6_in_sock(nam);
	} else
	/* scope issues will be handled in in6_setpeeraddr(). */
	error = in6_setpeeraddr(so, nam);

	return error;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splnet.
 */
void
in6_pcbnotify(pcbinfo, dst, fport_arg, src, lport_arg, cmd, cmdarg, notify)
	struct inpcbinfo *pcbinfo;
	struct sockaddr *dst;
	const struct sockaddr *src;
	u_int fport_arg, lport_arg;
	int cmd;
	void *cmdarg;
	struct inpcb *(*notify) __P((struct inpcb *, int));
{
	struct inpcbhead *head;
	struct inpcb *inp, *ninp;
	struct sockaddr_in6 sa6_src, *sa6_dst;
	u_short	fport = fport_arg, lport = lport_arg;
	u_int32_t flowinfo;
	int errno, s;

	if ((unsigned)cmd >= PRC_NCMDS || dst->sa_family != AF_INET6)
		return;

	sa6_dst = (struct sockaddr_in6 *)dst;
	if (IN6_IS_ADDR_UNSPECIFIED(&sa6_dst->sin6_addr))
		return;

	/*
	 * note that src can be NULL when we get notify by local fragmentation.
	 */
	sa6_src = (src == NULL) ? sa6_any : *(const struct sockaddr_in6 *)src;
	flowinfo = sa6_src.sin6_flowinfo;

	/*
	 * Redirects go to all references to the destination,
	 * and use in6_rtchange to invalidate the route cache.
	 * Dead host indications: also use in6_rtchange to invalidate
	 * the cache, and deliver the error to all the sockets.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		bzero((caddr_t)&sa6_src.sin6_addr, sizeof(sa6_src.sin6_addr));

		if (cmd != PRC_HOSTDEAD)
			notify = in6_rtchange;
	}
	errno = inet6ctlerrmap[cmd];
	s = splnet();
	head = pcbinfo->listhead;
	INP_INFO_WLOCK(pcbinfo);
 	for (inp = LIST_FIRST(head); inp != NULL; inp = ninp) {
		INP_LOCK(inp);
 		ninp = LIST_NEXT(inp, inp_list);

 		if ((inp->inp_vflag & INP_IPV6) == 0) {
			INP_UNLOCK(inp);
			continue;
		}

		/*
		 * If the error designates a new path MTU for a destination
		 * and the application (associated with this socket) wanted to
		 * know the value, notify. Note that we notify for all
		 * disconnected sockets if the corresponding application
		 * wanted. This is because some UDP applications keep sending
		 * sockets disconnected.
		 * XXX: should we avoid to notify the value to TCP sockets?
		 */
		if (cmd == PRC_MSGSIZE && (inp->inp_flags & IN6P_MTU) != 0 &&
		    (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) ||
		     IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, &sa6_dst->sin6_addr))) {
			ip6_notify_pmtu(inp, (struct sockaddr_in6 *)dst,
					(u_int32_t *)cmdarg);
		}

		/*
		 * Detect if we should notify the error. If no source and
		 * destination ports are specifed, but non-zero flowinfo and
		 * local address match, notify the error. This is the case
		 * when the error is delivered with an encrypted buffer
		 * by ESP. Otherwise, just compare addresses and ports
		 * as usual.
		 */
		if (lport == 0 && fport == 0 && flowinfo &&
		    inp->inp_socket != NULL &&
		    flowinfo == (inp->in6p_flowinfo & IPV6_FLOWLABEL_MASK) &&
		    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, &sa6_src.sin6_addr))
			goto do_notify;
		else if (!IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr,
					     &sa6_dst->sin6_addr) ||
			 inp->inp_socket == 0 ||
			 (lport && inp->inp_lport != lport) ||
			 (!IN6_IS_ADDR_UNSPECIFIED(&sa6_src.sin6_addr) &&
			  !IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr,
					      &sa6_src.sin6_addr)) ||
			 (fport && inp->inp_fport != fport)) {
			INP_UNLOCK(inp);
			continue;
		}

	  do_notify:
		if (notify) {
			if ((*notify)(inp, errno))
				INP_UNLOCK(inp);
		} else
			INP_UNLOCK(inp);
	}
	INP_INFO_WUNLOCK(pcbinfo);
	splx(s);
}

/*
 * Lookup a PCB based on the local address and port.
 */
struct inpcb *
in6_pcblookup_local(pcbinfo, laddr, lport_arg, wild_okay)
	struct inpcbinfo *pcbinfo;
	struct in6_addr *laddr;
	u_int lport_arg;
	int wild_okay;
{
	register struct inpcb *inp;
	int matchwild = 3, wildcard;
	u_short lport = lport_arg;

	if (!wild_okay) {
		struct inpcbhead *head;
		/*
		 * Look for an unconnected (wildcard foreign addr) PCB that
		 * matches the local address and port we're looking for.
		 */
		head = &pcbinfo->hashbase[INP_PCBHASH(INADDR_ANY, lport, 0,
						      pcbinfo->hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
			if ((inp->inp_vflag & INP_IPV6) == 0)
				continue;
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
			    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr) &&
			    inp->inp_lport == lport) {
				/*
				 * Found.
				 */
				return (inp);
			}
		}
		/*
		 * Not found.
		 */
		return (NULL);
	} else {
		struct inpcbporthead *porthash;
		struct inpcbport *phd;
		struct inpcb *match = NULL;
		/*
		 * Best fit PCB lookup.
		 *
		 * First see if this local port is in use by looking on the
		 * port hash list.
		 */
		porthash = &pcbinfo->porthashbase[INP_PCBPORTHASH(lport,
		    pcbinfo->porthashmask)];
		LIST_FOREACH(phd, porthash, phd_hash) {
			if (phd->phd_port == lport)
				break;
		}
		if (phd != NULL) {
			/*
			 * Port is in use by one or more PCBs. Look for best
			 * fit.
			 */
			LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
				wildcard = 0;
				if ((inp->inp_vflag & INP_IPV6) == 0)
					continue;
				if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr))
					wildcard++;
				if (!IN6_IS_ADDR_UNSPECIFIED(
					&inp->in6p_laddr)) {
					if (IN6_IS_ADDR_UNSPECIFIED(laddr))
						wildcard++;
					else if (!IN6_ARE_ADDR_EQUAL(
						&inp->in6p_laddr, laddr))
						continue;
				} else {
					if (!IN6_IS_ADDR_UNSPECIFIED(laddr))
						wildcard++;
				}
				if (wildcard < matchwild) {
					match = inp;
					matchwild = wildcard;
					if (matchwild == 0) {
						break;
					}
				}
			}
		}
		return (match);
	}
}

void
in6_pcbpurgeif0(head, ifp)
	struct in6pcb *head;
	struct ifnet *ifp;
{
	struct in6pcb *in6p;
	struct ip6_moptions *im6o;
	struct in6_multi_mship *imm, *nimm;

	for (in6p = head; in6p != NULL; in6p = LIST_NEXT(in6p, inp_list)) {
		im6o = in6p->in6p_moptions;
		if ((in6p->inp_vflag & INP_IPV6) &&
		    im6o) {
			/*
			 * Unselect the outgoing interface if it is being
			 * detached.
			 */
			if (im6o->im6o_multicast_ifp == ifp)
				im6o->im6o_multicast_ifp = NULL;

			/*
			 * Drop multicast group membership if we joined
			 * through the interface being detached.
			 * XXX controversial - is it really legal for kernel
			 * to force this?
			 */
			for (imm = im6o->im6o_memberships.lh_first;
			     imm != NULL; imm = nimm) {
				nimm = imm->i6mm_chain.le_next;
				if (imm->i6mm_maddr->in6m_ifp == ifp) {
					LIST_REMOVE(imm, i6mm_chain);
					in6_delmulti(imm->i6mm_maddr);
					free(imm, M_IPMADDR);
				}
			}
		}
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in6_losing(in6p)
	struct inpcb *in6p;
{
	/*
	 * We don't store route pointers in the routing table anymore
	 */
	return;
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
struct inpcb *
in6_rtchange(inp, errno)
	struct inpcb *inp;
	int errno;
{
	/*
	 * We don't store route pointers in the routing table anymore
	 */
	return inp;
}

/*
 * Lookup PCB in hash list.
 */
struct inpcb *
in6_pcblookup_hash(pcbinfo, faddr, fport_arg, laddr, lport_arg, wildcard, ifp)
	struct inpcbinfo *pcbinfo;
	struct in6_addr *faddr, *laddr;
	u_int fport_arg, lport_arg;
	int wildcard;
	struct ifnet *ifp;
{
	struct inpcbhead *head;
	register struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;
	int faith;

	if (faithprefix_p != NULL)
		faith = (*faithprefix_p)(laddr);
	else
		faith = 0;

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->hashbase[INP_PCBHASH(faddr->s6_addr32[3] /* XXX */,
					      lport, fport,
					      pcbinfo->hashmask)];
	LIST_FOREACH(inp, head, inp_hash) {
		if ((inp->inp_vflag & INP_IPV6) == 0)
			continue;
		if (IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, faddr) &&
		    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr) &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport) {
			/*
			 * Found.
			 */
			return (inp);
		}
	}
	if (wildcard) {
		struct inpcb *local_wild = NULL;

		head = &pcbinfo->hashbase[INP_PCBHASH(INADDR_ANY, lport, 0,
						      pcbinfo->hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
			if ((inp->inp_vflag & INP_IPV6) == 0)
				continue;
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
			    inp->inp_lport == lport) {
				if (faith && (inp->inp_flags & INP_FAITH) == 0)
					continue;
				if (IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr,
						       laddr))
					return (inp);
				else if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
					local_wild = inp;
			}
		}
		return (local_wild);
	}

	/*
	 * Not found.
	 */
	return (NULL);
}

void
init_sin6(struct sockaddr_in6 *sin6, struct mbuf *m)
{
	struct ip6_hdr *ip;

	ip = mtod(m, struct ip6_hdr *);
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = ip->ip6_src;

	(void)sa6_recoverscope(sin6); /* XXX: should catch errors... */

	return;
}
