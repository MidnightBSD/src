/*-
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2009 Robert N. M. Watson
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
 *	@(#)ipx.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/rwlock.h>
#include <sys/sockio.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_var.h>

/*
 * The IPX-layer address list is protected by ipx_ifaddr_rw.
 */
struct rwlock		 ipx_ifaddr_rw;
struct ipx_ifaddrhead	 ipx_ifaddrhead;

static void	ipx_ifscrub(struct ifnet *ifp, struct ipx_ifaddr *ia);
static int	ipx_ifinit(struct ifnet *ifp, struct ipx_ifaddr *ia,
		    struct sockaddr_ipx *sipx, int scrub);

/*
 * Generic internet control operations (ioctl's).
 */
int
ipx_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp,
    struct thread *td)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct ipx_aliasreq *ifra = (struct ipx_aliasreq *)data;
	struct ipx_ifaddr *ia;
	struct ifaddr *ifa;
	int dstIsNew, hostIsNew;
	int error, priv;

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp == NULL)
		return (EADDRNOTAVAIL);

	IPX_IFADDR_RLOCK();
	TAILQ_FOREACH(ia, &ipx_ifaddrhead, ia_link) {
		if (ia->ia_ifp == ifp)
			break;
	}
	if (ia != NULL)
		ifa_ref(&ia->ia_ifa);
	IPX_IFADDR_RUNLOCK();

	error = 0;
	switch (cmd) {
	case SIOCGIFADDR:
		if (ia == NULL) {
			error = EADDRNOTAVAIL;
			goto out;
		}
		*(struct sockaddr_ipx *)&ifr->ifr_addr = ia->ia_addr;
		goto out;

	case SIOCGIFBRDADDR:
		if (ia == NULL) {
			error = EADDRNOTAVAIL;
			goto out;
		}
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EINVAL;
			goto out;
		}
		*(struct sockaddr_ipx *)&ifr->ifr_dstaddr = ia->ia_broadaddr;
		goto out;

	case SIOCGIFDSTADDR:
		if (ia == NULL) {
			error = EADDRNOTAVAIL;
			goto out;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
			goto out;
		}
		*(struct sockaddr_ipx *)&ifr->ifr_dstaddr = ia->ia_dstaddr;
		goto out;
	}

	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCDIFADDR:
		priv = (cmd == SIOCAIFADDR) ? PRIV_NET_ADDIFADDR :
		    PRIV_NET_DELIFADDR;
		if (td && (error = priv_check(td, priv)) != 0)
			goto out;

		IPX_IFADDR_RLOCK();
		if (ifra->ifra_addr.sipx_family == AF_IPX) {
			struct ipx_ifaddr *oia;

			for (oia = ia; ia; ia = TAILQ_NEXT(ia, ia_link)) {
				if (ia->ia_ifp == ifp  &&
				    ipx_neteq(ia->ia_addr.sipx_addr,
				    ifra->ifra_addr.sipx_addr))
					break;
			}
			if (oia != NULL && oia != ia)
				ifa_free(&oia->ia_ifa);
			if (ia != NULL && oia != ia)
				ifa_ref(&ia->ia_ifa);
		}
		IPX_IFADDR_RUNLOCK();
		if (cmd == SIOCDIFADDR && ia == NULL) {
			error = EADDRNOTAVAIL;
			goto out;
		}
		/* FALLTHROUGH */

	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
		if (td && (error = priv_check(td, PRIV_NET_SETLLADDR)) != 0)
			goto out;
		if (ia == NULL) {
			ia = malloc(sizeof(*ia), M_IFADDR, M_NOWAIT | M_ZERO);
			if (ia == NULL) {
				error = ENOBUFS;
				goto out;
			}
			ifa = (struct ifaddr *)ia;
			ifa_init(ifa);
			ia->ia_ifp = ifp;
			ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ifa->ifa_netmask = (struct sockaddr *)&ipx_netmask;
			ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sipx_family = AF_IPX;
				ia->ia_broadaddr.sipx_len =
				    sizeof(ia->ia_addr);
				ia->ia_broadaddr.sipx_addr.x_host =
				    ipx_broadhost;
			}
			ifa_ref(&ia->ia_ifa);		/* ipx_ifaddrhead */
			IPX_IFADDR_WLOCK();
			TAILQ_INSERT_TAIL(&ipx_ifaddrhead, ia, ia_link);
			IPX_IFADDR_WUNLOCK();

			ifa_ref(&ia->ia_ifa);		/* if_addrhead */
			IF_ADDR_WLOCK(ifp);
			TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);
			IF_ADDR_WUNLOCK(ifp);
		}
		break;

	default:
		if (td && (error = priv_check(td, PRIV_NET_HWIOCTL)) != 0)
			goto out;
	}

	switch (cmd) {
	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
			goto out;
		}
		if (ia->ia_flags & IFA_ROUTE) {
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_flags &= ~IFA_ROUTE;
		}
		if (ifp->if_ioctl) {
			error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR,
			    (void *)ia);
			if (error)
				goto out;
		}
		*(struct sockaddr *)&ia->ia_dstaddr = ifr->ifr_dstaddr;
		goto out;

	case SIOCSIFADDR:
		error = ipx_ifinit(ifp, ia,
		    (struct sockaddr_ipx *)&ifr->ifr_addr, 1);
		goto out;

	case SIOCDIFADDR:
		ipx_ifscrub(ifp, ia);
		ifa = (struct ifaddr *)ia;

		IF_ADDR_WLOCK(ifp);
		TAILQ_REMOVE(&ifp->if_addrhead, ifa, ifa_link);
		IF_ADDR_WUNLOCK(ifp);
		ifa_free(ifa);				/* if_addrhead */

		IPX_IFADDR_WLOCK();
		TAILQ_REMOVE(&ipx_ifaddrhead, ia, ia_link);
		IPX_IFADDR_WUNLOCK();
		ifa_free(&ia->ia_ifa);			/* ipx_ifaddrhead */
		goto out;

	case SIOCAIFADDR:
		dstIsNew = 0;
		hostIsNew = 1;
		if (ia->ia_addr.sipx_family == AF_IPX) {
			if (ifra->ifra_addr.sipx_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ipx_neteq(ifra->ifra_addr.sipx_addr,
					 ia->ia_addr.sipx_addr))
				hostIsNew = 0;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sipx_family == AF_IPX)) {
			if (hostIsNew == 0)
				ipx_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			dstIsNew  = 1;
		}
		if (ifra->ifra_addr.sipx_family == AF_IPX &&
					    (hostIsNew || dstIsNew))
			error = ipx_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		goto out;

	default:
		if (ifp->if_ioctl == NULL) {
			error = EOPNOTSUPP;
			goto out;
		}
		error = ((*ifp->if_ioctl)(ifp, cmd, data));
	}

out:
	if (ia != NULL)
		ifa_free(&ia->ia_ifa);
	return (error);
}

/*
 * Delete any previous route for an old address.
 */
static void
ipx_ifscrub(struct ifnet *ifp, struct ipx_ifaddr *ia)
{

	if (ia->ia_flags & IFA_ROUTE) {
		if (ifp->if_flags & IFF_POINTOPOINT) {
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
		} else
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
		ia->ia_flags &= ~IFA_ROUTE;
	}
}

/*
 * Initialize an interface's internet address and routing table entry.
 */
static int
ipx_ifinit(struct ifnet *ifp, struct ipx_ifaddr *ia,
    struct sockaddr_ipx *sipx, int scrub)
{
	struct sockaddr_ipx oldaddr;
	int s = splimp(), error;

	/*
	 * Set up new addresses.
	 */
	oldaddr = ia->ia_addr;
	ia->ia_addr = *sipx;

	/*
	 * The convention we shall adopt for naming is that a supplied
	 * address of zero means that "we don't care".  Use the MAC address
	 * of the interface.  If it is an interface without a MAC address,
	 * like a serial line, the address must be supplied.
	 *
	 * Give the interface a chance to initialize if this is its first
	 * address, and to validate the address if necessary.
	 */
	if (ifp->if_ioctl != NULL &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (void *)ia))) {
		ia->ia_addr = oldaddr;
		splx(s);
		return (error);
	}
	splx(s);
	ia->ia_ifa.ifa_metric = ifp->if_metric;

	/*
	 * Add route for the network.
	 */
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		ipx_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	if (ifp->if_flags & IFF_POINTOPOINT)
		rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
	else {
		ia->ia_broadaddr.sipx_addr.x_net = ia->ia_addr.sipx_addr.x_net;
		rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_UP);
	}
	ia->ia_flags |= IFA_ROUTE;
	return (0);
}

/*
 * Return address info for specified internet network.
 */
struct ipx_ifaddr *
ipx_iaonnetof(struct ipx_addr *dst)
{
	struct ipx_ifaddr *ia;
	struct ipx_addr *compare;
	struct ifnet *ifp;
	struct ipx_ifaddr *ia_maybe = NULL;
	union ipx_net net = dst->x_net;

	IPX_IFADDR_LOCK_ASSERT();

	TAILQ_FOREACH(ia, &ipx_ifaddrhead, ia_link) {
		if ((ifp = ia->ia_ifp) != NULL) {
			if (ifp->if_flags & IFF_POINTOPOINT) {
				compare = &satoipx_addr(ia->ia_dstaddr);
				if (ipx_hosteq(*dst, *compare))
					return (ia);
				if (ipx_neteqnn(net,
				    ia->ia_addr.sipx_addr.x_net))
					ia_maybe = ia;
			} else {
				if (ipx_neteqnn(net,
				    ia->ia_addr.sipx_addr.x_net))
					return (ia);
			}
		}
	}
	return (ia_maybe);
}

void
ipx_printhost(struct ipx_addr *addr)
{
	u_short port;
	struct ipx_addr work = *addr;
	char *p; u_char *q;
	char *net = "", *host = "";
	char cport[10], chost[15], cnet[15];

	port = ntohs(work.x_port);

	if (ipx_nullnet(work) && ipx_nullhost(work)) {
		if (port)
			printf("*.%x", port);
		else
			printf("*.*");

		return;
	}

	if (ipx_wildnet(work))
		net = "any";
	else if (ipx_nullnet(work))
		net = "*";
	else {
		q = work.x_net.c_net;
		snprintf(cnet, sizeof(cnet), "%x%x%x%x",
			q[0], q[1], q[2], q[3]);
		for (p = cnet; *p == '0' && p < cnet + 8; p++)
			continue;
		net = p;
	}

	if (ipx_wildhost(work))
		host = "any";
	else if (ipx_nullhost(work))
		host = "*";
	else {
		q = work.x_host.c_host;
		snprintf(chost, sizeof(chost), "%x%x%x%x%x%x",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}

	if (port) {
		if (strcmp(host, "*") == 0) {
			host = "";
			snprintf(cport, sizeof(cport), "%x", port);
		} else
			snprintf(cport, sizeof(cport), ".%x", port);
	} else
		*cport = 0;

	printf("%s.%s%s", net, host, cport);
}
