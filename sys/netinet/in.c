/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
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
 *	@(#)in.c	8.4 (Berkeley) 1/9/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/netinet/in.c 174854 2007-12-22 06:32:46Z cvs2svn $");

#include "opt_carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

static int in_mask2len(struct in_addr *);
static void in_len2mask(struct in_addr *, int);
static int in_lifaddr_ioctl(struct socket *, u_long, caddr_t,
	struct ifnet *, struct thread *);

static int	in_addprefix(struct in_ifaddr *, int);
static int	in_scrubprefix(struct in_ifaddr *);
static void	in_socktrim(struct sockaddr_in *);
static int	in_ifinit(struct ifnet *,
	    struct in_ifaddr *, struct sockaddr_in *, int);
static void	in_purgemaddrs(struct ifnet *);

static int subnetsarelocal = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, subnets_are_local, CTLFLAG_RW,
	&subnetsarelocal, 0, "Treat all subnets as directly connected");
static int sameprefixcarponly = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, same_prefix_carp_only, CTLFLAG_RW,
	&sameprefixcarponly, 0,
	"Refuse to create same prefixes on different interfaces");

extern struct inpcbinfo ripcbinfo;
extern struct inpcbinfo udbinfo;

/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(struct in_addr in)
{
	register u_long i = ntohl(in.s_addr);
	register struct in_ifaddr *ia;

	if (subnetsarelocal) {
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
			if ((i & ia->ia_netmask) == ia->ia_net)
				return (1);
	} else {
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
			if ((i & ia->ia_subnetmask) == ia->ia_subnet)
				return (1);
	}
	return (0);
}

/*
 * Return 1 if an internet address is for the local host and configured
 * on one of its interfaces.
 */
int
in_localip(struct in_addr in)
{
	struct in_ifaddr *ia;

	LIST_FOREACH(ia, INADDR_HASH(in.s_addr), ia_hash) {
		if (IA_SIN(ia)->sin_addr.s_addr == in.s_addr)
			return 1;
	}
	return 0;
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	register u_long i = ntohl(in.s_addr);
	register u_long net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i) || IN_LINKLOCAL(i))
		return (0);
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Trim a mask in a sockaddr
 */
static void
in_socktrim(struct sockaddr_in *ap)
{
    register char *cplim = (char *) &ap->sin_addr;
    register char *cp = (char *) (&ap->sin_addr + 1);

    ap->sin_len = 0;
    while (--cp >= cplim)
	if (*cp) {
	    (ap)->sin_len = cp - (char *) (ap) + 1;
	    break;
	}
}

static int
in_mask2len(mask)
	struct in_addr *mask;
{
	int x, y;
	u_char *p;

	p = (u_char *)mask;
	for (x = 0; x < sizeof(*mask); x++) {
		if (p[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < 8; y++) {
			if ((p[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * 8 + y;
}

static void
in_len2mask(struct in_addr *mask, int len)
{
	int i;
	u_char *p;

	p = (u_char *)mask;
	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		p[i] = 0xff;
	if (len % 8)
		p[i] = (0xff00 >> (len % 8)) & 0xff;
}

/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp,
    struct thread *td)
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct in_ifaddr *ia = 0, *iap;
	register struct ifaddr *ifa;
	struct in_addr allhosts_addr;
	struct in_addr dst;
	struct in_ifaddr *oia;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error, hostIsNew, iaIsNew, maskIsNew, s;
	int iaIsFirst;

	iaIsFirst = 0;
	iaIsNew = 0;
	allhosts_addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);

	switch (cmd) {
	case SIOCALIFADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_ADDIFADDR);
			if (error)
				return (error);
		}
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, td);

	case SIOCDLIFADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_DELIFADDR);
			if (error)
				return (error);
		}
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, td);

	case SIOCGLIFADDR:
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, td);
	}

	/*
	 * Find address for this interface, if it exists.
	 *
	 * If an alias address was specified, find that one instead of
	 * the first one on the interface, if possible.
	 */
	if (ifp) {
		dst = ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
		LIST_FOREACH(iap, INADDR_HASH(dst.s_addr), ia_hash)
			if (iap->ia_ifp == ifp &&
			    iap->ia_addr.sin_addr.s_addr == dst.s_addr) {
				ia = iap;
				break;
			}
		if (ia == NULL)
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				iap = ifatoia(ifa);
				if (iap->ia_addr.sin_family == AF_INET) {
					ia = iap;
					break;
				}
			}
		if (ia == NULL)
			iaIsFirst = 1;
	}

	switch (cmd) {

	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifp == 0)
			return (EADDRNOTAVAIL);
		if (ifra->ifra_addr.sin_family == AF_INET) {
			for (oia = ia; ia; ia = TAILQ_NEXT(ia, ia_link)) {
				if (ia->ia_ifp == ifp  &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifra->ifra_addr.sin_addr.s_addr)
					break;
			}
			if ((ifp->if_flags & IFF_POINTOPOINT)
			    && (cmd == SIOCAIFADDR)
			    && (ifra->ifra_dstaddr.sin_addr.s_addr
				== INADDR_ANY)) {
				return EDESTADDRREQ;
			}
		}
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_ADDIFADDR);
			if (error)
				return (error);
		}

		if (ifp == 0)
			return (EADDRNOTAVAIL);
		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK | M_ZERO);
			if (ia == (struct in_ifaddr *)NULL)
				return (ENOBUFS);
			/*
			 * Protect from ipintr() traversing address list
			 * while we're modifying it.
			 */
			s = splnet();
			ifa = &ia->ia_ifa;
			IFA_LOCK_INIT(ifa);
			ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
			ifa->ifa_refcnt = 1;
			TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

			ia->ia_sockmask.sin_len = 8;
			ia->ia_sockmask.sin_family = AF_INET;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;

			TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);
			splx(s);
			iaIsNew = 1;
		}
		break;

	case SIOCSIFBRDADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_ADDIFADDR);
			if (error)
				return (error);
		}
		/* FALLTHROUGH */

	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia == (struct in_ifaddr *)0)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_addr;
		return (0);

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		return (0);

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		return (0);

	case SIOCGIFNETMASK:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_sockmask;
		return (0);

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *(struct sockaddr_in *)&ifr->ifr_dstaddr;
		if (ifp->if_ioctl) {
			IFF_LOCKGIANT(ifp);
			error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR,
			    (caddr_t)ia);
			IFF_UNLOCKGIANT(ifp);
			if (error) {
				ia->ia_dstaddr = oldaddr;
				return (error);
			}
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr =
					(struct sockaddr *)&ia->ia_dstaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		return (0);

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ia->ia_broadaddr = *(struct sockaddr_in *)&ifr->ifr_broadaddr;
		return (0);

	case SIOCSIFADDR:
		error = in_ifinit(ifp, ia,
		    (struct sockaddr_in *) &ifr->ifr_addr, 1);
		if (error != 0 && iaIsNew)
			break;
		if (error == 0) {
			if (iaIsFirst && (ifp->if_flags & IFF_MULTICAST) != 0)
				in_addmulti(&allhosts_addr, ifp);
			EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		}
		return (0);

	case SIOCSIFNETMASK:
		ia->ia_sockmask.sin_addr = ifra->ifra_addr.sin_addr;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr);
		return (0);

	case SIOCAIFADDR:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
					       ia->ia_addr.sin_addr.s_addr)
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_sockmask.sin_family = AF_INET;
			ia->ia_subnetmask =
			     ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
		    (hostIsNew || maskIsNew))
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if (error != 0 && iaIsNew)
			break;

		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		if (error == 0) {
			if (iaIsFirst && (ifp->if_flags & IFF_MULTICAST) != 0)
				in_addmulti(&allhosts_addr, ifp);
			EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		}
		return (error);

	case SIOCDIFADDR:
		/*
		 * in_ifscrub kills the interface route.
		 */
		in_ifscrub(ifp, ia);
		/*
		 * in_ifadown gets rid of all the rest of
		 * the routes.  This is not quite the right
		 * thing to do, but at least if we are running
		 * a routing process they will come back.
		 */
		in_ifadown(&ia->ia_ifa, 1);
		EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		error = 0;
		break;

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
		return (error);
	}

	/*
	 * Protect from ipintr() traversing address list while we're modifying
	 * it.
	 */
	s = splnet();
	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
	if (ia->ia_addr.sin_family == AF_INET) {
		LIST_REMOVE(ia, ia_hash);
		/*
		 * If this is the last IPv4 address configured on this
		 * interface, leave the all-hosts group.
		 * XXX: This is quite ugly because of locking and structure.
		 */
		oia = NULL;
		IFP_TO_IA(ifp, oia);
		if (oia == NULL) {
			struct in_multi *inm;

			IFF_LOCKGIANT(ifp);
			IN_MULTI_LOCK();
			IN_LOOKUP_MULTI(allhosts_addr, ifp, inm);
			if (inm != NULL)
				in_delmulti_locked(inm);
			IN_MULTI_UNLOCK();
			IFF_UNLOCKGIANT(ifp);
		}
	}
	IFAFREE(&ia->ia_ifa);
	splx(s);

	return (error);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (?!?)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		EINVAL since we can't deduce hostid part of the address.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in_ioctl()
 */
static int
in_lifaddr_ioctl(struct socket *so, u_long cmd, caddr_t data,
    struct ifnet *ifp, struct thread *td)
{
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in_lifaddr_ioctl");
		/*NOTRECHED*/
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/*FALLTHROUGH*/
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		if (iflr->addr.ss_family != AF_INET)
			return EINVAL;
		if (iflr->addr.ss_len != sizeof(struct sockaddr_in))
			return EINVAL;
		/* XXX need improvement */
		if (iflr->dstaddr.ss_family
		 && iflr->dstaddr.ss_family != AF_INET)
			return EINVAL;
		if (iflr->dstaddr.ss_family
		 && iflr->dstaddr.ss_len != sizeof(struct sockaddr_in))
			return EINVAL;
		break;
	default: /*shouldn't happen*/
		return EOPNOTSUPP;
	}
	if (sizeof(struct in_addr) * 8 < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in_aliasreq ifra;

		if (iflr->flags & IFLR_PREFIX)
			return EINVAL;

		/* copy args to in_aliasreq, perform ioctl(SIOCAIFADDR_IN6). */
		bzero(&ifra, sizeof(ifra));
		bcopy(iflr->iflr_name, ifra.ifra_name,
			sizeof(ifra.ifra_name));

		bcopy(&iflr->addr, &ifra.ifra_addr, iflr->addr.ss_len);

		if (iflr->dstaddr.ss_family) {	/*XXX*/
			bcopy(&iflr->dstaddr, &ifra.ifra_dstaddr,
				iflr->dstaddr.ss_len);
		}

		ifra.ifra_mask.sin_family = AF_INET;
		ifra.ifra_mask.sin_len = sizeof(struct sockaddr_in);
		in_len2mask(&ifra.ifra_mask.sin_addr, iflr->prefixlen);

		return in_control(so, SIOCAIFADDR, (caddr_t)&ifra, ifp, td);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in_ifaddr *ia;
		struct in_addr mask, candidate, match;
		struct sockaddr_in *sin;

		bzero(&mask, sizeof(mask));
		bzero(&match, sizeof(match));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in_len2mask(&mask, iflr->prefixlen);

			sin = (struct sockaddr_in *)&iflr->addr;
			match.s_addr = sin->sin_addr.s_addr;
			match.s_addr &= mask.s_addr;

			/* if you set extra bits, that's wrong */
			if (match.s_addr != sin->sin_addr.s_addr)
				return EINVAL;

		} else {
			/* on getting an address, take the 1st match */
			/* on deleting an address, do exact match */
			if (cmd != SIOCGLIFADDR) {
				in_len2mask(&mask, 32);
				sin = (struct sockaddr_in *)&iflr->addr;
				match.s_addr = sin->sin_addr.s_addr;
			}
		}

		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)	{
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			if (match.s_addr == 0)
				break;
			candidate.s_addr = ((struct sockaddr_in *)&ifa->ifa_addr)->sin_addr.s_addr;
			candidate.s_addr &= mask.s_addr;
			if (candidate.s_addr == match.s_addr)
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia = (struct in_ifaddr *)ifa;

		if (cmd == SIOCGLIFADDR) {
			/* fill in the if_laddrreq structure */
			bcopy(&ia->ia_addr, &iflr->addr, ia->ia_addr.sin_len);

			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &iflr->dstaddr,
					ia->ia_dstaddr.sin_len);
			} else
				bzero(&iflr->dstaddr, sizeof(iflr->dstaddr));

			iflr->prefixlen =
				in_mask2len(&ia->ia_sockmask.sin_addr);

			iflr->flags = 0;	/*XXX*/

			return 0;
		} else {
			struct in_aliasreq ifra;

			/* fill in_aliasreq and do ioctl(SIOCDIFADDR_IN6) */
			bzero(&ifra, sizeof(ifra));
			bcopy(iflr->iflr_name, ifra.ifra_name,
				sizeof(ifra.ifra_name));

			bcopy(&ia->ia_addr, &ifra.ifra_addr,
				ia->ia_addr.sin_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &ifra.ifra_dstaddr,
					ia->ia_dstaddr.sin_len);
			}
			bcopy(&ia->ia_sockmask, &ifra.ifra_dstaddr,
				ia->ia_sockmask.sin_len);

			return in_control(so, SIOCDIFADDR, (caddr_t)&ifra,
					  ifp, td);
		}
	    }
	}

	return EOPNOTSUPP;	/*just for safety*/
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia)
{

	in_scrubprefix(ia);
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
static int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin,
    int scrub)
{
	register u_long i = ntohl(sin->sin_addr.s_addr);
	struct sockaddr_in oldaddr;
	int s = splimp(), flags = RTF_UP, error = 0;

	oldaddr = ia->ia_addr;
	if (oldaddr.sin_family == AF_INET)
		LIST_REMOVE(ia, ia_hash);
	ia->ia_addr = *sin;
	if (ia->ia_addr.sin_family == AF_INET)
		LIST_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
		    ia, ia_hash);
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl) {
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia);
		IFF_UNLOCKGIANT(ifp);
		if (error) {
			splx(s);
			/* LIST_REMOVE(ia, ia_hash) is done in in_control */
			ia->ia_addr = oldaddr;
			if (ia->ia_addr.sin_family == AF_INET)
				LIST_INSERT_HEAD(INADDR_HASH(
				    ia->ia_addr.sin_addr.s_addr), ia, ia_hash);
			return (error);
		}
	}
	splx(s);
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
#ifdef DEV_CARP
	/*
	 * XXX: carp(4) does not have interface route
	 */
	if (ifp->if_type == IFT_CARP)
		return (0);
#endif
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
			htonl(ia->ia_subnet | ~ia->ia_subnetmask);
		ia->ia_netbroadcast.s_addr =
			htonl(ia->ia_net | ~ ia->ia_netmask);
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_dstaddr = ia->ia_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			return (0);
		flags |= RTF_HOST;
	}
	if ((error = in_addprefix(ia, flags)) != 0)
		return (error);

	return (error);
}

#define rtinitflags(x) \
	((((x)->ia_ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) != 0) \
	    ? RTF_HOST : 0)
/*
 * Check if we have a route for the given prefix already or add one accordingly.
 */
static int
in_addprefix(struct in_ifaddr *target, int flags)
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	int error;

	if ((flags & RTF_HOST) != 0) {
		prefix = target->ia_dstaddr.sin_addr;
		mask.s_addr = 0;
	} else {
		prefix = target->ia_addr.sin_addr;
		mask = target->ia_sockmask.sin_addr;
		prefix.s_addr &= mask.s_addr;
	}

	TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
		if (rtinitflags(ia)) {
			p = ia->ia_addr.sin_addr;

			if (prefix.s_addr != p.s_addr)
				continue;
		} else {
			p = ia->ia_addr.sin_addr;
			m = ia->ia_sockmask.sin_addr;
			p.s_addr &= m.s_addr;

			if (prefix.s_addr != p.s_addr ||
			    mask.s_addr != m.s_addr)
				continue;
		}

		/*
		 * If we got a matching prefix route inserted by other
		 * interface address, we are done here.
		 */
		if (ia->ia_flags & IFA_ROUTE) {
			if (sameprefixcarponly &&
			    target->ia_ifp->if_type != IFT_CARP &&
			    ia->ia_ifp->if_type != IFT_CARP)
				return (EEXIST);
			else
				return (0);
		}
	}

	/*
	 * No-one seem to have this prefix route, so we try to insert it.
	 */
	error = rtinit(&target->ia_ifa, (int)RTM_ADD, flags);
	if (!error)
		target->ia_flags |= IFA_ROUTE;
	return error;
}

/*
 * If there is no other address in the system that can serve a route to the
 * same prefix, remove the route.  Hand over the route to the new address
 * otherwise.
 */
static int
in_scrubprefix(struct in_ifaddr *target)
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p;
	int error;

	if ((target->ia_flags & IFA_ROUTE) == 0)
		return 0;

	if (rtinitflags(target))
		prefix = target->ia_dstaddr.sin_addr;
	else {
		prefix = target->ia_addr.sin_addr;
		mask = target->ia_sockmask.sin_addr;
		prefix.s_addr &= mask.s_addr;
	}

	TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
		if (rtinitflags(ia))
			p = ia->ia_dstaddr.sin_addr;
		else {
			p = ia->ia_addr.sin_addr;
			p.s_addr &= ia->ia_sockmask.sin_addr.s_addr;
		}

		if (prefix.s_addr != p.s_addr)
			continue;

		/*
		 * If we got a matching prefix address, move IFA_ROUTE and
		 * the route itself to it.  Make sure that routing daemons
		 * get a heads-up.
		 *
		 * XXX: a special case for carp(4) interface
		 */
		if ((ia->ia_flags & IFA_ROUTE) == 0
#ifdef DEV_CARP
		    && (ia->ia_ifp->if_type != IFT_CARP)
#endif
							) {
			rtinit(&(target->ia_ifa), (int)RTM_DELETE,
			    rtinitflags(target));
			target->ia_flags &= ~IFA_ROUTE;

			error = rtinit(&ia->ia_ifa, (int)RTM_ADD,
			    rtinitflags(ia) | RTF_UP);
			if (error == 0)
				ia->ia_flags |= IFA_ROUTE;
			return error;
		}
	}

	/*
	 * As no-one seem to have this prefix, we can remove the route.
	 */
	rtinit(&(target->ia_ifa), (int)RTM_DELETE, rtinitflags(target));
	target->ia_flags &= ~IFA_ROUTE;
	return 0;
}

#undef rtinitflags

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	register struct ifaddr *ifa;
	u_long t;

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		return 0;
	t = ntohl(in.s_addr);
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
#define ia ((struct in_ifaddr *)ifa)
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
		     in.s_addr == ia->ia_netbroadcast.s_addr ||
		     /*
		      * Check for old-style (host 0) broadcast.
		      */
		     t == ia->ia_subnet || t == ia->ia_net) &&
		     /*
		      * Check for an all one subnetmask. These
		      * only exist when an interface gets a secondary
		      * address.
		      */
		     ia->ia_subnetmask != (u_long)0xffffffff)
			    return 1;
	return (0);
#undef ia
}

/*
 * Delete all IPv4 multicast address records, and associated link-layer
 * multicast address records, associated with ifp.
 */
static void
in_purgemaddrs(struct ifnet *ifp)
{
	struct in_multi *inm;
	struct in_multi *oinm;

#ifdef DIAGNOSTIC
	printf("%s: purging ifp %p\n", __func__, ifp);
#endif
	IFF_LOCKGIANT(ifp);
	IN_MULTI_LOCK();
	LIST_FOREACH_SAFE(inm, &in_multihead, inm_link, oinm) {
		if (inm->inm_ifp == ifp)
			in_delmulti_locked(inm);
	}
	IN_MULTI_UNLOCK();
	IFF_UNLOCKGIANT(ifp);
}

/*
 * On interface removal, clean up IPv4 data structures hung off of the ifnet.
 */
void
in_ifdetach(struct ifnet *ifp)
{

	in_pcbpurgeif0(&ripcbinfo, ifp);
	in_pcbpurgeif0(&udbinfo, ifp);
	in_purgemaddrs(ifp);
}
