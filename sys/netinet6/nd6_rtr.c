/*	$FreeBSD: src/sys/netinet6/nd6_rtr.c,v 1.26.2.2 2005/11/04 20:48:12 ume Exp $	*/
/*	$KAME: nd6_rtr.c,v 1.111 2001/04/27 01:37:15 jinmei Exp $	*/

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
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/radix.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>
#include <netinet6/scope6_var.h>

#include <net/net_osdep.h>

#define SDL(s)	((struct sockaddr_dl *)s)

static struct nd_defrouter *defrtrlist_update __P((struct nd_defrouter *));
static struct in6_ifaddr *in6_ifadd __P((struct nd_prefix *,
	struct in6_addr *));
static struct nd_pfxrouter *pfxrtr_lookup __P((struct nd_prefix *,
	struct nd_defrouter *));
static void pfxrtr_add __P((struct nd_prefix *, struct nd_defrouter *));
static void pfxrtr_del __P((struct nd_pfxrouter *));
static struct nd_pfxrouter *find_pfxlist_reachable_router
	__P((struct nd_prefix *));
static void defrouter_addifreq __P((struct ifnet *));
static void nd6_rtmsg __P((int, struct rtentry *));

static void in6_init_address_ltimes __P((struct nd_prefix *,
	struct in6_addrlifetime *));

static int rt6_deleteroute __P((struct radix_node *, void *));

extern int nd6_recalc_reachtm_interval;

static struct ifnet *nd6_defifp;
int nd6_defifindex;

int ip6_use_tempaddr = 0;

int ip6_desync_factor;
u_int32_t ip6_temp_preferred_lifetime = DEF_TEMP_PREFERRED_LIFETIME;
u_int32_t ip6_temp_valid_lifetime = DEF_TEMP_VALID_LIFETIME;
/*
 * shorter lifetimes for debugging purposes.
int ip6_temp_preferred_lifetime = 800;
static int ip6_temp_valid_lifetime = 1800;
*/
int ip6_temp_regen_advance = TEMPADDR_REGEN_ADVANCE;

/*
 * Receive Router Solicitation Message - just for routers.
 * Router solicitation/advertisement is mostly managed by userland program
 * (rtadvd) so here we have no function like nd6_ra_output().
 *
 * Based on RFC 2461
 */
void
nd6_rs_input(m, off, icmp6len)
	struct	mbuf *m;
	int off, icmp6len;
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_solicit *nd_rs;
	struct in6_addr saddr6 = ip6->ip6_src;
	char *lladdr = NULL;
	int lladdrlen = 0;
#if 0
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)NULL;
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)NULL;
	struct rtentry *rt = NULL;
	int is_newentry;
#endif
	union nd_opts ndopts;

	/* If I'm not a router, ignore it. */
	if (ip6_accept_rtadv != 0 || ip6_forwarding != 1)
		goto freeit;

	/* Sanity checks */
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_rs_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim, ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&ip6->ip6_dst), if_name(ifp)));
		goto bad;
	}

	/*
	 * Don't update the neighbor cache, if src = ::.
	 * This indicates that the src has no IP address assigned yet.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&saddr6))
		goto freeit;

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, icmp6len,);
	nd_rs = (struct nd_router_solicit *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(nd_rs, struct nd_router_solicit *, m, off, icmp6len);
	if (nd_rs == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}
#endif

	icmp6len -= sizeof(*nd_rs);
	nd6_option_init(nd_rs + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: lladdrlen mismatch for %s "
		    "(if %d, RS packet %d)\n",
		    ip6_sprintf(&saddr6),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_SOLICIT, 0);

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat.icp6s_badrs++;
	m_freem(m);
}

/*
 * Receive Router Advertisement Message.
 *
 * Based on RFC 2461
 * TODO: on-link bit on prefix information
 * TODO: ND_RA_FLAG_{OTHER,MANAGED} processing
 */
void
nd6_ra_input(m, off, icmp6len)
	struct	mbuf *m;
	int off, icmp6len;
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_advert *nd_ra;
	struct in6_addr saddr6 = ip6->ip6_src;
#if 0
	struct in6_addr daddr6 = ip6->ip6_dst;
	int flags; /* = nd_ra->nd_ra_flags_reserved; */
	int is_managed = ((flags & ND_RA_FLAG_MANAGED) != 0);
	int is_other = ((flags & ND_RA_FLAG_OTHER) != 0);
#endif
	union nd_opts ndopts;
	struct nd_defrouter *dr;

	/*
	 * We only accept RAs only when
	 * the system-wide variable allows the acceptance, and
	 * per-interface variable allows RAs on the receiving interface.
	 */
	if (ip6_accept_rtadv == 0)
		goto freeit;
	if (!(ndi->flags & ND6_IFF_ACCEPT_RTADV))
		goto freeit;

	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim, ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&ip6->ip6_dst), if_name(ifp)));
		goto bad;
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&saddr6)) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: src %s is not link-local\n",
		    ip6_sprintf(&saddr6)));
		goto bad;
	}

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, icmp6len,);
	nd_ra = (struct nd_router_advert *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(nd_ra, struct nd_router_advert *, m, off, icmp6len);
	if (nd_ra == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}
#endif

	icmp6len -= sizeof(*nd_ra);
	nd6_option_init(nd_ra + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

    {
	struct nd_defrouter dr0;
	u_int32_t advreachable = nd_ra->nd_ra_reachable;

	dr0.rtaddr = saddr6;
	dr0.flags  = nd_ra->nd_ra_flags_reserved;
	dr0.rtlifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	dr0.expire = time_second + dr0.rtlifetime;
	dr0.ifp = ifp;
	/* unspecified or not? (RFC 2461 6.3.4) */
	if (advreachable) {
		advreachable = ntohl(advreachable);
		if (advreachable <= MAX_REACHABLE_TIME &&
		    ndi->basereachable != advreachable) {
			ndi->basereachable = advreachable;
			ndi->reachable = ND_COMPUTE_RTIME(ndi->basereachable);
			ndi->recalctm = nd6_recalc_reachtm_interval; /* reset */
		}
	}
	if (nd_ra->nd_ra_retransmit)
		ndi->retrans = ntohl(nd_ra->nd_ra_retransmit);
	if (nd_ra->nd_ra_curhoplimit)
		ndi->chlim = nd_ra->nd_ra_curhoplimit;
	dr = defrtrlist_update(&dr0);
    }

	/*
	 * prefix
	 */
	if (ndopts.nd_opts_pi) {
		struct nd_opt_hdr *pt;
		struct nd_opt_prefix_info *pi = NULL;
		struct nd_prefix pr;

		for (pt = (struct nd_opt_hdr *)ndopts.nd_opts_pi;
		     pt <= (struct nd_opt_hdr *)ndopts.nd_opts_pi_end;
		     pt = (struct nd_opt_hdr *)((caddr_t)pt +
						(pt->nd_opt_len << 3))) {
			if (pt->nd_opt_type != ND_OPT_PREFIX_INFORMATION)
				continue;
			pi = (struct nd_opt_prefix_info *)pt;

			if (pi->nd_opt_pi_len != 4) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid option "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_len));
				continue;
			}

			if (128 < pi->nd_opt_pi_prefix_len) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_prefix_len));
				continue;
			}

			if (IN6_IS_ADDR_MULTICAST(&pi->nd_opt_pi_prefix)
			 || IN6_IS_ADDR_LINKLOCAL(&pi->nd_opt_pi_prefix)) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "%s, ignored\n",
				    ip6_sprintf(&pi->nd_opt_pi_prefix)));
				continue;
			}

			/* aggregatable unicast address, rfc2374 */
			if ((pi->nd_opt_pi_prefix.s6_addr8[0] & 0xe0) == 0x20
			 && pi->nd_opt_pi_prefix_len != 64) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefixlen "
				    "%d for rfc2374 prefix %s, ignored\n",
				    pi->nd_opt_pi_prefix_len,
				    ip6_sprintf(&pi->nd_opt_pi_prefix)));
				continue;
			}

			bzero(&pr, sizeof(pr));
			pr.ndpr_prefix.sin6_family = AF_INET6;
			pr.ndpr_prefix.sin6_len = sizeof(pr.ndpr_prefix);
			pr.ndpr_prefix.sin6_addr = pi->nd_opt_pi_prefix;
			pr.ndpr_ifp = (struct ifnet *)m->m_pkthdr.rcvif;

			pr.ndpr_raf_onlink = (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK) ? 1 : 0;
			pr.ndpr_raf_auto = (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_AUTO) ? 1 : 0;
			pr.ndpr_plen = pi->nd_opt_pi_prefix_len;
			pr.ndpr_vltime = ntohl(pi->nd_opt_pi_valid_time);
			pr.ndpr_pltime = ntohl(pi->nd_opt_pi_preferred_time);
			if (in6_init_prefix_ltimes(&pr))
				continue; /* prefix lifetime init failed */
			(void)prelist_update(&pr, dr, m);
		}
	}

	/*
	 * MTU
	 */
	if (ndopts.nd_opts_mtu && ndopts.nd_opts_mtu->nd_opt_mtu_len == 1) {
		u_long mtu;
		u_long maxmtu;

		mtu = (u_long)ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);

		/* lower bound */
		if (mtu < IPV6_MMTU) {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu option "
			    "mtu=%lu sent from %s, ignoring\n",
			    mtu, ip6_sprintf(&ip6->ip6_src)));
			goto skip;
		}

		/* upper bound */
		maxmtu = (ndi->maxmtu && ndi->maxmtu < ifp->if_mtu)
		    ? ndi->maxmtu : ifp->if_mtu;
		if (mtu <= maxmtu) {
			int change = (ndi->linkmtu != mtu);

			ndi->linkmtu = mtu;
			if (change) /* in6_maxmtu may change */
				in6_setmaxmtu();
		} else {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu "
			    "mtu=%lu sent from %s; "
			    "exceeds maxmtu %lu, ignoring\n",
			    mtu, ip6_sprintf(&ip6->ip6_src), maxmtu));
		}
	}

 skip:

	/*
	 * Source link layer address
	 */
    {
	char *lladdr = NULL;
	int lladdrlen = 0;

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: lladdrlen mismatch for %s "
		    "(if %d, RA packet %d)\n", ip6_sprintf(&saddr6),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr,
	    lladdrlen, ND_ROUTER_ADVERT, 0);

	/*
	 * Installing a link-layer address might change the state of the
	 * router's neighbor cache, which might also affect our on-link
	 * detection of adveritsed prefixes.
	 */
	pfxlist_onlink_check();
    }

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat.icp6s_badra++;
	m_freem(m);
}

/*
 * default router list proccessing sub routines
 */

/* tell the change to user processes watching the routing socket. */
static void
nd6_rtmsg(cmd, rt)
	int cmd;
	struct rtentry *rt;
{
	struct rt_addrinfo info;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	info.rti_info[RTAX_IFP] =
		(struct sockaddr *)TAILQ_FIRST(&rt->rt_ifp->if_addrlist);
	info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;

	rt_missmsg(cmd, &info, rt->rt_flags, 0);
}

void
defrouter_addreq(new)
	struct nd_defrouter *new;
{
	struct sockaddr_in6 def, mask, gate;
	struct rtentry *newrt = NULL;

	bzero(&def, sizeof(def));
	bzero(&mask, sizeof(mask));
	bzero(&gate, sizeof(gate));

	def.sin6_len = mask.sin6_len = gate.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6_family = mask.sin6_family = gate.sin6_family = AF_INET6;
	gate.sin6_addr = new->rtaddr;

	(void)rtrequest(RTM_ADD, (struct sockaddr *)&def,
	    (struct sockaddr *)&gate, (struct sockaddr *)&mask,
	    RTF_GATEWAY, &newrt);
	if (newrt) {
		RT_LOCK(newrt);
		nd6_rtmsg(RTM_ADD, newrt); /* tell user process */
		RT_REMREF(newrt);
		RT_UNLOCK(newrt);
	}
	return;
}

/* Add a route to a given interface as default */
void
defrouter_addifreq(ifp)
	struct ifnet *ifp;
{
	struct sockaddr_in6 def, mask;
	struct ifaddr *ifa;
	struct rtentry *newrt = NULL;
	int error, flags;

	bzero(&def, sizeof(def));
	bzero(&mask, sizeof(mask));

	def.sin6_len = mask.sin6_len = sizeof(struct sockaddr_in6);
	def.sin6_family = mask.sin6_family = AF_INET6;

	/*
	 * Search for an ifaddr beloging to the specified interface.
	 * XXX: An IPv6 address are required to be assigned on the interface.
	 */
	if ((ifa = ifaof_ifpforaddr((struct sockaddr *)&def, ifp)) == NULL) {
		nd6log((LOG_ERR,	/* better error? */
		    "defrouter_addifreq: failed to find an ifaddr "
		    "to install a route to interface %s\n",
		    if_name(ifp)));
		return;
	}

	flags = ifa->ifa_flags;
	error = rtrequest(RTM_ADD, (struct sockaddr *)&def, ifa->ifa_addr,
	    (struct sockaddr *)&mask, flags, &newrt);
	if (error != 0) {
		nd6log((LOG_ERR,
		    "defrouter_addifreq: failed to install a route to "
		    "interface %s (errno = %d)\n",
		    if_name(ifp), error));
	} else {
		if (newrt) {
			RT_LOCK(newrt);
			nd6_rtmsg(RTM_ADD, newrt);
			RT_REMREF(newrt);
			RT_UNLOCK(newrt);
		}
	}
}

struct nd_defrouter *
defrouter_lookup(addr, ifp)
	struct in6_addr *addr;
	struct ifnet *ifp;
{
	struct nd_defrouter *dr;

	for (dr = TAILQ_FIRST(&nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		if (dr->ifp == ifp && IN6_ARE_ADDR_EQUAL(addr, &dr->rtaddr))
			return (dr);
	}

	return (NULL);		/* search failed */
}

void
defrouter_delreq(dr, dofree)
	struct nd_defrouter *dr;
	int dofree;
{
	struct sockaddr_in6 def, mask, gate;
	struct rtentry *oldrt = NULL;

	bzero(&def, sizeof(def));
	bzero(&mask, sizeof(mask));
	bzero(&gate, sizeof(gate));

	def.sin6_len = mask.sin6_len = gate.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6_family = mask.sin6_family = gate.sin6_family = AF_INET6;
	gate.sin6_addr = dr->rtaddr;

	rtrequest(RTM_DELETE, (struct sockaddr *)&def,
	    (struct sockaddr *)&gate,
	    (struct sockaddr *)&mask, RTF_GATEWAY, &oldrt);
	if (oldrt) {
		nd6_rtmsg(RTM_DELETE, oldrt);
		RTFREE(oldrt);
	}

	if (dofree)		/* XXX: necessary? */
		free(dr, M_IP6NDP);
}

void
defrtrlist_del(dr)
	struct nd_defrouter *dr;
{
	struct nd_defrouter *deldr = NULL;
	struct nd_prefix *pr;

	/*
	 * Flush all the routing table entries that use the router
	 * as a next hop.
	 */
	if (!ip6_forwarding && ip6_accept_rtadv) /* XXX: better condition? */
		rt6_flush(&dr->rtaddr, dr->ifp);

	if (dr == TAILQ_FIRST(&nd_defrouter))
		deldr = dr;	/* The router is primary. */

	TAILQ_REMOVE(&nd_defrouter, dr, dr_entry);

	/*
	 * Also delete all the pointers to the router in each prefix lists.
	 */
	for (pr = nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		struct nd_pfxrouter *pfxrtr;
		if ((pfxrtr = pfxrtr_lookup(pr, dr)) != NULL)
			pfxrtr_del(pfxrtr);
	}
	pfxlist_onlink_check();

	/*
	 * If the router is the primary one, choose a new one.
	 * Note that defrouter_select() will remove the current gateway
	 * from the routing table.
	 */
	if (deldr)
		defrouter_select();

	free(dr, M_IP6NDP);
}

/*
 * Default Router Selection according to Section 6.3.6 of RFC 2461:
 * 1) Routers that are reachable or probably reachable should be
 *    preferred.
 * 2) When no routers on the list are known to be reachable or
 *    probably reachable, routers SHOULD be selected in a round-robin
 *    fashion.
 * 3) If the Default Router List is empty, assume that all
 *    destinations are on-link.
 */
void
defrouter_select()
{
	int s = splnet();
	struct nd_defrouter *dr, anydr;
	struct rtentry *rt = NULL;
	struct llinfo_nd6 *ln = NULL;

	/*
	 * Search for a (probably) reachable router from the list.
	 */
	for (dr = TAILQ_FIRST(&nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		if ((rt = nd6_lookup(&dr->rtaddr, 0, dr->ifp)) &&
		    (ln = (struct llinfo_nd6 *)rt->rt_llinfo) &&
		    ND6_IS_LLINFO_PROBREACH(ln)) {
			/* Got it, and move it to the head */
			TAILQ_REMOVE(&nd_defrouter, dr, dr_entry);
			TAILQ_INSERT_HEAD(&nd_defrouter, dr, dr_entry);
			break;
		}
	}

	if ((dr = TAILQ_FIRST(&nd_defrouter))) {
		/*
		 * De-install the previous default gateway and install
		 * a new one.
		 * Note that if there is no reachable router in the list,
		 * the head entry will be used anyway.
		 * XXX: do we have to check the current routing table entry?
		 */
		bzero(&anydr, sizeof(anydr));
		defrouter_delreq(&anydr, 0);
		defrouter_addreq(dr);
	}
	else {
		/*
		 * The Default Router List is empty, so install the default
		 * route to an inteface.
		 * XXX: The specification does not say this mechanism should
		 * be restricted to hosts, but this would be not useful
		 * (even harmful) for routers.
		 */
		if (!ip6_forwarding) {
			/*
			 * De-install the current default route
			 * in advance.
			 */
			bzero(&anydr, sizeof(anydr));
			defrouter_delreq(&anydr, 0);
			if (nd6_defifp) {
				/*
				 * Install a route to the default interface
				 * as default route.
				 * XXX: we enable this for host only, because
				 * this may override a default route installed
				 * a user process (e.g. routing daemon) in a
				 * router case.
				 */
				defrouter_addifreq(nd6_defifp);
			} else {
				nd6log((LOG_INFO, "defrouter_select: "
				    "there's no default router and no default"
				    " interface\n"));
			}
		}
	}

	splx(s);
	return;
}

static struct nd_defrouter *
defrtrlist_update(new)
	struct nd_defrouter *new;
{
	struct nd_defrouter *dr, *n;
	int s = splnet();

	if ((dr = defrouter_lookup(&new->rtaddr, new->ifp)) != NULL) {
		/* entry exists */
		if (new->rtlifetime == 0) {
			defrtrlist_del(dr);
			dr = NULL;
		} else {
			/* override */
			dr->flags = new->flags; /* xxx flag check */
			dr->rtlifetime = new->rtlifetime;
			dr->expire = new->expire;
		}
		splx(s);
		return (dr);
	}

	/* entry does not exist */
	if (new->rtlifetime == 0) {
		splx(s);
		return (NULL);
	}

	n = (struct nd_defrouter *)malloc(sizeof(*n), M_IP6NDP, M_NOWAIT);
	if (n == NULL) {
		splx(s);
		return (NULL);
	}
	bzero(n, sizeof(*n));
	*n = *new;

	/*
	 * Insert the new router at the end of the Default Router List.
	 * If there is no other router, install it anyway. Otherwise,
	 * just continue to use the current default router.
	 */
	TAILQ_INSERT_TAIL(&nd_defrouter, n, dr_entry);
	if (TAILQ_FIRST(&nd_defrouter) == n)
		defrouter_select();
	splx(s);

	return (n);
}

static struct nd_pfxrouter *
pfxrtr_lookup(pr, dr)
	struct nd_prefix *pr;
	struct nd_defrouter *dr;
{
	struct nd_pfxrouter *search;

	for (search = pr->ndpr_advrtrs.lh_first; search; search = search->pfr_next) {
		if (search->router == dr)
			break;
	}

	return (search);
}

static void
pfxrtr_add(pr, dr)
	struct nd_prefix *pr;
	struct nd_defrouter *dr;
{
	struct nd_pfxrouter *new;

	new = (struct nd_pfxrouter *)malloc(sizeof(*new), M_IP6NDP, M_NOWAIT);
	if (new == NULL)
		return;
	bzero(new, sizeof(*new));
	new->router = dr;

	LIST_INSERT_HEAD(&pr->ndpr_advrtrs, new, pfr_entry);

	pfxlist_onlink_check();
}

static void
pfxrtr_del(pfr)
	struct nd_pfxrouter *pfr;
{
	LIST_REMOVE(pfr, pfr_entry);
	free(pfr, M_IP6NDP);
}

struct nd_prefix *
nd6_prefix_lookup(pr)
	struct nd_prefix *pr;
{
	struct nd_prefix *search;

	for (search = nd_prefix.lh_first; search; search = search->ndpr_next) {
		if (pr->ndpr_ifp == search->ndpr_ifp &&
		    pr->ndpr_plen == search->ndpr_plen &&
		    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
		    &search->ndpr_prefix.sin6_addr, pr->ndpr_plen)) {
			break;
		}
	}

	return (search);
}

int
nd6_prelist_add(pr, dr, newp)
	struct nd_prefix *pr, **newp;
	struct nd_defrouter *dr;
{
	struct nd_prefix *new = NULL;
	int i, s;

	new = (struct nd_prefix *)malloc(sizeof(*new), M_IP6NDP, M_NOWAIT);
	if (new == NULL)
		return(ENOMEM);
	bzero(new, sizeof(*new));
	*new = *pr;
	if (newp != NULL)
		*newp = new;

	/* initialization */
	LIST_INIT(&new->ndpr_advrtrs);
	in6_prefixlen2mask(&new->ndpr_mask, new->ndpr_plen);
	/* make prefix in the canonical form */
	for (i = 0; i < 4; i++)
		new->ndpr_prefix.sin6_addr.s6_addr32[i] &=
		    new->ndpr_mask.s6_addr32[i];

	s = splnet();
	/* link ndpr_entry to nd_prefix list */
	LIST_INSERT_HEAD(&nd_prefix, new, ndpr_entry);
	splx(s);

	/* ND_OPT_PI_FLAG_ONLINK processing */
	if (new->ndpr_raf_onlink) {
		int e;

		if ((e = nd6_prefix_onlink(new)) != 0) {
			nd6log((LOG_ERR, "nd6_prelist_add: failed to make "
			    "the prefix %s/%d on-link on %s (errno=%d)\n",
			    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
			    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
			/* proceed anyway. XXX: is it correct? */
		}
	}

	if (dr)
		pfxrtr_add(new, dr);

	return 0;
}

void
prelist_remove(pr)
	struct nd_prefix *pr;
{
	struct nd_pfxrouter *pfr, *next;
	int e, s;

	/* make sure to invalidate the prefix until it is really freed. */
	pr->ndpr_vltime = 0;
	pr->ndpr_pltime = 0;
#if 0
	/*
	 * Though these flags are now meaningless, we'd rather keep the value
	 * not to confuse users when executing "ndp -p".
	 */
	pr->ndpr_raf_onlink = 0;
	pr->ndpr_raf_auto = 0;
#endif
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0 &&
	    (e = nd6_prefix_offlink(pr)) != 0) {
		nd6log((LOG_ERR, "prelist_remove: failed to make %s/%d offlink "
		    "on %s, errno=%d\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
		/* what should we do? */
	}

	if (pr->ndpr_refcnt > 0)
		return;		/* notice here? */

	s = splnet();

	/* unlink ndpr_entry from nd_prefix list */
	LIST_REMOVE(pr, ndpr_entry);

	/* free list of routers that adversed the prefix */
	for (pfr = pr->ndpr_advrtrs.lh_first; pfr; pfr = next) {
		next = pfr->pfr_next;

		free(pfr, M_IP6NDP);
	}
	splx(s);

	free(pr, M_IP6NDP);

	pfxlist_onlink_check();
}

int
prelist_update(new, dr, m)
	struct nd_prefix *new;
	struct nd_defrouter *dr; /* may be NULL */
	struct mbuf *m;
{
	struct in6_ifaddr *ia6 = NULL, *ia6_match = NULL;
	struct ifaddr *ifa;
	struct ifnet *ifp = new->ndpr_ifp;
	struct nd_prefix *pr;
	int s = splnet();
	int error = 0;
	int newprefix = 0;
	int auth;
	struct in6_addrlifetime lt6_tmp;

	auth = 0;
	if (m) {
		/*
		 * Authenticity for NA consists authentication for
		 * both IP header and IP datagrams, doesn't it ?
		 */
#if defined(M_AUTHIPHDR) && defined(M_AUTHIPDGM)
		auth = ((m->m_flags & M_AUTHIPHDR) &&
		    (m->m_flags & M_AUTHIPDGM));
#endif
	}

	if ((pr = nd6_prefix_lookup(new)) != NULL) {
		/*
		 * nd6_prefix_lookup() ensures that pr and new have the same
		 * prefix on a same interface.
		 */

		/*
		 * Update prefix information.  Note that the on-link (L) bit
		 * and the autonomous (A) bit should NOT be changed from 1
		 * to 0.
		 */
		if (new->ndpr_raf_onlink == 1)
			pr->ndpr_raf_onlink = 1;
		if (new->ndpr_raf_auto == 1)
			pr->ndpr_raf_auto = 1;
		if (new->ndpr_raf_onlink) {
			pr->ndpr_vltime = new->ndpr_vltime;
			pr->ndpr_pltime = new->ndpr_pltime;
			pr->ndpr_preferred = new->ndpr_preferred;
			pr->ndpr_expire = new->ndpr_expire;
		}

		if (new->ndpr_raf_onlink &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
			int e;

			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "prelist_update: failed to make "
				    "the prefix %s/%d on-link on %s "
				    "(errno=%d)\n",
				    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
				/* proceed anyway. XXX: is it correct? */
			}
		}

		if (dr && pfxrtr_lookup(pr, dr) == NULL)
			pfxrtr_add(pr, dr);
	} else {
		struct nd_prefix *newpr = NULL;

		newprefix = 1;

		if (new->ndpr_vltime == 0)
			goto end;
		if (new->ndpr_raf_onlink == 0 && new->ndpr_raf_auto == 0)
			goto end;

		bzero(&new->ndpr_addr, sizeof(struct in6_addr));

		error = nd6_prelist_add(new, dr, &newpr);
		if (error != 0 || newpr == NULL) {
			nd6log((LOG_NOTICE, "prelist_update: "
			    "nd6_prelist_add failed for %s/%d on %s "
			    "errno=%d, returnpr=%p\n",
			    ip6_sprintf(&new->ndpr_prefix.sin6_addr),
			    new->ndpr_plen, if_name(new->ndpr_ifp),
			    error, newpr));
			goto end; /* we should just give up in this case. */
		}

		/*
		 * XXX: from the ND point of view, we can ignore a prefix
		 * with the on-link bit being zero.  However, we need a
		 * prefix structure for references from autoconfigured
		 * addresses.  Thus, we explicitly make sure that the prefix
		 * itself expires now.
		 */
		if (newpr->ndpr_raf_onlink == 0) {
			newpr->ndpr_vltime = 0;
			newpr->ndpr_pltime = 0;
			in6_init_prefix_ltimes(newpr);
		}

		pr = newpr;
	}

	/*
	 * Address autoconfiguration based on Section 5.5.3 of RFC 2462.
	 * Note that pr must be non NULL at this point.
	 */

	/* 5.5.3 (a). Ignore the prefix without the A bit set. */
	if (!new->ndpr_raf_auto)
		goto afteraddrconf;

	/*
	 * 5.5.3 (b). the link-local prefix should have been ignored in
	 * nd6_ra_input.
	 */

	/*
	 * 5.5.3 (c). Consistency check on lifetimes: pltime <= vltime.
	 * This should have been done in nd6_ra_input.
	 */

 	/*
	 * 5.5.3 (d). If the prefix advertised does not match the prefix of an
	 * address already in the list, and the Valid Lifetime is not 0,
	 * form an address.  Note that even a manually configured address
	 * should reject autoconfiguration of a new address.
	 */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		struct in6_ifaddr *ifa6;
		int ifa_plen;
		u_int32_t storedlifetime;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ifa6 = (struct in6_ifaddr *)ifa;

		/*
		 * Spec is not clear here, but I believe we should concentrate
		 * on unicast (i.e. not anycast) addresses.
		 * XXX: other ia6_flags? detached or duplicated?
		 */
		if ((ifa6->ia6_flags & IN6_IFF_ANYCAST) != 0)
			continue;

		ifa_plen = in6_mask2len(&ifa6->ia_prefixmask.sin6_addr, NULL);
		if (ifa_plen != new->ndpr_plen ||
		    !in6_are_prefix_equal(&ifa6->ia_addr.sin6_addr,
		    &new->ndpr_prefix.sin6_addr, ifa_plen))
			continue;

		if (ia6_match == NULL) /* remember the first one */
			ia6_match = ifa6;

		if ((ifa6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
			continue;

		/*
		 * An already autoconfigured address matched.  Now that we
		 * are sure there is at least one matched address, we can
		 * proceed to 5.5.3. (e): update the lifetimes according to the
		 * "two hours" rule and the privacy extension.
		 */
#define TWOHOUR		(120*60)
		lt6_tmp = ifa6->ia6_lifetime;

		if (lt6_tmp.ia6t_vltime == ND6_INFINITE_LIFETIME)
			storedlifetime = ND6_INFINITE_LIFETIME;
		else if (IFA6_IS_INVALID(ifa6))
			storedlifetime = 0;
		else
			storedlifetime = lt6_tmp.ia6t_expire - time_second;

		/* when not updating, keep the current stored lifetime. */
		lt6_tmp.ia6t_vltime = storedlifetime;

		if (TWOHOUR < new->ndpr_vltime ||
		    storedlifetime < new->ndpr_vltime) {
			lt6_tmp.ia6t_vltime = new->ndpr_vltime;
		} else if (storedlifetime <= TWOHOUR
#if 0
			   /*
			    * This condition is logically redundant, so we just
			    * omit it.
			    * See IPng 6712, 6717, and 6721.
			    */
			   && new->ndpr_vltime <= storedlifetime
#endif
			) {
			if (auth) {
				lt6_tmp.ia6t_vltime = new->ndpr_vltime;
			}
		} else {
			/*
			 * new->ndpr_vltime <= TWOHOUR &&
			 * TWOHOUR < storedlifetime
			 */
			lt6_tmp.ia6t_vltime = TWOHOUR;
		}

		/* The 2 hour rule is not imposed for preferred lifetime. */
		lt6_tmp.ia6t_pltime = new->ndpr_pltime;

		in6_init_address_ltimes(pr, &lt6_tmp);

		/*
		 * When adjusting the lifetimes of an existing temporary
		 * address, only lower the lifetimes.
		 * RFC 3041 3.3. (1).
		 * XXX: how should we modify ia6t_[pv]ltime?
		 */
		if ((ifa6->ia6_flags & IN6_IFF_TEMPORARY) != 0) {
			if (lt6_tmp.ia6t_expire == 0 || /* no expire */
			    lt6_tmp.ia6t_expire >
			    ifa6->ia6_lifetime.ia6t_expire) {
				lt6_tmp.ia6t_expire =
				    ifa6->ia6_lifetime.ia6t_expire;
			}
			if (lt6_tmp.ia6t_preferred == 0 || /* no expire */
			    lt6_tmp.ia6t_preferred >
			    ifa6->ia6_lifetime.ia6t_preferred) {
				lt6_tmp.ia6t_preferred =
				    ifa6->ia6_lifetime.ia6t_preferred;
			}
		}

		ifa6->ia6_lifetime = lt6_tmp;
	}
	if (ia6_match == NULL && new->ndpr_vltime) {
		/*
		 * No address matched and the valid lifetime is non-zero.
		 * Create a new address.
		 */
		if ((ia6 = in6_ifadd(new, NULL)) != NULL) {
			/*
			 * note that we should use pr (not new) for reference.
			 */
			pr->ndpr_refcnt++;
			ia6->ia6_ndpr = pr;

			/*
			 * RFC 3041 3.3 (2).
			 * When a new public address is created as described
			 * in RFC2462, also create a new temporary address.
			 *
			 * RFC 3041 3.5.
			 * When an interface connects to a new link, a new
			 * randomized interface identifier should be generated
			 * immediately together with a new set of temporary
			 * addresses.  Thus, we specifiy 1 as the 2nd arg of
			 * in6_tmpifadd().
			 */
			if (ip6_use_tempaddr) {
				int e;
				if ((e = in6_tmpifadd(ia6, 1)) != 0) {
					nd6log((LOG_NOTICE, "prelist_update: "
					    "failed to create a temporary "
					    "address, errno=%d\n",
					    e));
				}
			}

			/*
			 * A newly added address might affect the status
			 * of other addresses, so we check and update it.
			 * XXX: what if address duplication happens?
			 */
			pfxlist_onlink_check();
		} else {
			/* just set an error. do not bark here. */
			error = EADDRNOTAVAIL; /* XXX: might be unused. */
		}
	}

  afteraddrconf:

 end:
	splx(s);
	return error;
}

/*
 * A supplement function used in the on-link detection below;
 * detect if a given prefix has a (probably) reachable advertising router.
 * XXX: lengthy function name...
 */
static struct nd_pfxrouter *
find_pfxlist_reachable_router(pr)
	struct nd_prefix *pr;
{
	struct nd_pfxrouter *pfxrtr;
	struct rtentry *rt;
	struct llinfo_nd6 *ln;

	for (pfxrtr = LIST_FIRST(&pr->ndpr_advrtrs); pfxrtr;
	     pfxrtr = LIST_NEXT(pfxrtr, pfr_entry)) {
		if ((rt = nd6_lookup(&pfxrtr->router->rtaddr, 0,
		    pfxrtr->router->ifp)) &&
		    (ln = (struct llinfo_nd6 *)rt->rt_llinfo) &&
		    ND6_IS_LLINFO_PROBREACH(ln))
			break;	/* found */
	}

	return (pfxrtr);
}

/*
 * Check if each prefix in the prefix list has at least one available router
 * that advertised the prefix (a router is "available" if its neighbor cache
 * entry is reachable or probably reachable).
 * If the check fails, the prefix may be off-link, because, for example,
 * we have moved from the network but the lifetime of the prefix has not
 * expired yet.  So we should not use the prefix if there is another prefix
 * that has an available router.
 * But, if there is no prefix that has an available router, we still regards
 * all the prefixes as on-link.  This is because we can't tell if all the
 * routers are simply dead or if we really moved from the network and there
 * is no router around us.
 */
void
pfxlist_onlink_check()
{
	struct nd_prefix *pr;
	struct in6_ifaddr *ifa;

	/*
	 * Check if there is a prefix that has a reachable advertising
	 * router.
	 */
	for (pr = nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		if (pr->ndpr_raf_onlink && find_pfxlist_reachable_router(pr))
			break;
	}

	if (pr) {
		/*
		 * There is at least one prefix that has a reachable router.
		 * Detach prefixes which have no reachable advertising
		 * router, and attach other prefixes.
		 */
		for (pr = nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
			/* XXX: a link-local prefix should never be detached */
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			/*
			 * we aren't interested in prefixes without the L bit
			 * set.
			 */
			if (pr->ndpr_raf_onlink == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
			    find_pfxlist_reachable_router(pr) == NULL)
				pr->ndpr_stateflags |= NDPRF_DETACHED;
			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
			    find_pfxlist_reachable_router(pr) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	} else {
		/* there is no prefix that has a reachable router */
		for (pr = nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			if (pr->ndpr_raf_onlink == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	}

	/*
	 * Remove each interface route associated with a (just) detached
	 * prefix, and reinstall the interface route for a (just) attached
	 * prefix.  Note that all attempt of reinstallation does not
	 * necessarily success, when a same prefix is shared among multiple
	 * interfaces.  Such cases will be handled in nd6_prefix_onlink,
	 * so we don't have to care about them.
	 */
	for (pr = nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		int e;

		if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
			continue;

		if (pr->ndpr_raf_onlink == 0)
			continue;

		if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
			if ((e = nd6_prefix_offlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d offlink, errno=%d\n",
				    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, e));
			}
		}
		if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0 &&
		    pr->ndpr_raf_onlink) {
			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d offlink, errno=%d\n",
				    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, e));
			}
		}
	}

	/*
	 * Changes on the prefix status might affect address status as well.
	 * Make sure that all addresses derived from an attached prefix are
	 * attached, and that all addresses derived from a detached prefix are
	 * detached.  Note, however, that a manually configured address should
	 * always be attached.
	 * The precise detection logic is same as the one for prefixes.
	 */
	for (ifa = in6_ifaddr; ifa; ifa = ifa->ia_next) {
		if (!(ifa->ia6_flags & IN6_IFF_AUTOCONF))
			continue;

		if (ifa->ia6_ndpr == NULL) {
			/*
			 * This can happen when we first configure the address
			 * (i.e. the address exists, but the prefix does not).
			 * XXX: complicated relationships...
			 */
			continue;
		}

		if (find_pfxlist_reachable_router(ifa->ia6_ndpr))
			break;
	}
	if (ifa) {
		for (ifa = in6_ifaddr; ifa; ifa = ifa->ia_next) {
			if ((ifa->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			if (ifa->ia6_ndpr == NULL) /* XXX: see above. */
				continue;

			if (find_pfxlist_reachable_router(ifa->ia6_ndpr))
				ifa->ia6_flags &= ~IN6_IFF_DETACHED;
			else
				ifa->ia6_flags |= IN6_IFF_DETACHED;
		}
	}
	else {
		for (ifa = in6_ifaddr; ifa; ifa = ifa->ia_next) {
			if ((ifa->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			ifa->ia6_flags &= ~IN6_IFF_DETACHED;
		}
	}
}

int
nd6_prefix_onlink(pr)
	struct nd_prefix *pr;
{
	struct ifaddr *ifa;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct sockaddr_in6 mask6;
	struct nd_prefix *opr;
	u_long rtflags;
	int error = 0;
	struct rtentry *rt = NULL;

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_onlink: %s/%d is already on-link\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr), pr->ndpr_plen);
		return (EEXIST));
	}

	/*
	 * Add the interface route associated with the prefix.  Before
	 * installing the route, check if there's the same prefix on another
	 * interface, and the prefix has already installed the interface route.
	 * Although such a configuration is expected to be rare, we explicitly
	 * allow it.
	 */
	for (opr = nd_prefix.lh_first; opr; opr = opr->ndpr_next) {
		if (opr == pr)
			continue;

		if ((opr->ndpr_stateflags & NDPRF_ONLINK) == 0)
			continue;

		if (opr->ndpr_plen == pr->ndpr_plen &&
		    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
		    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen))
			return (0);
	}

	/*
	 * We prefer link-local addresses as the associated interface address.
	 */
	/* search for a link-local addr */
	ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp,
	    IN6_IFF_NOTREADY | IN6_IFF_ANYCAST);
	if (ifa == NULL) {
		/* XXX: freebsd does not have ifa_ifwithaf */
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == AF_INET6)
				break;
		}
		/* should we care about ia6_flags? */
	}
	if (ifa == NULL) {
		/*
		 * This can still happen, when, for example, we receive an RA
		 * containing a prefix with the L bit set and the A bit clear,
		 * after removing all IPv6 addresses on the receiving
		 * interface.  This should, of course, be rare though.
		 */
		nd6log((LOG_NOTICE,
		    "nd6_prefix_onlink: failed to find any ifaddr"
		    " to add route for a prefix(%s/%d) on %s\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(ifp)));
		return (0);
	}

	/*
	 * in6_ifinit() sets nd6_rtrequest to ifa_rtrequest for all ifaddrs.
	 * ifa->ifa_rtrequest = nd6_rtrequest;
	 */
	bzero(&mask6, sizeof(mask6));
	mask6.sin6_len = sizeof(mask6);
	mask6.sin6_addr = pr->ndpr_mask;
	rtflags = ifa->ifa_flags | RTF_CLONING | RTF_UP;
	if (nd6_need_cache(ifp)) {
		/* explicitly set in case ifa_flags does not set the flag. */
		rtflags |= RTF_CLONING;
	} else {
		/*
		 * explicitly clear the cloning bit in case ifa_flags sets it.
		 */
		rtflags &= ~RTF_CLONING;
	}
	error = rtrequest(RTM_ADD, (struct sockaddr *)&pr->ndpr_prefix,
	    ifa->ifa_addr, (struct sockaddr *)&mask6, rtflags, &rt);
	if (error == 0) {
		if (rt != NULL) /* this should be non NULL, though */
			nd6_rtmsg(RTM_ADD, rt);
		pr->ndpr_stateflags |= NDPRF_ONLINK;
	} else {
		nd6log((LOG_ERR, "nd6_prefix_onlink: failed to add route for a"
		    " prefix (%s/%d) on %s, gw=%s, mask=%s, flags=%lx "
		    "errno = %d\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(ifp),
		    ip6_sprintf(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr),
		    ip6_sprintf(&mask6.sin6_addr), rtflags, error));
	}

	if (rt != NULL) {
		RT_LOCK(rt);
		RT_REMREF(rt);
		RT_UNLOCK(rt);
	}

	return (error);
}

int
nd6_prefix_offlink(pr)
	struct nd_prefix *pr;
{
	int error = 0;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct nd_prefix *opr;
	struct sockaddr_in6 sa6, mask6;
	struct rtentry *rt = NULL;

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: %s/%d is already off-link\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr), pr->ndpr_plen));
		return (EEXIST);
	}

	bzero(&sa6, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	sa6.sin6_len = sizeof(sa6);
	bcopy(&pr->ndpr_prefix.sin6_addr, &sa6.sin6_addr,
	    sizeof(struct in6_addr));
	bzero(&mask6, sizeof(mask6));
	mask6.sin6_family = AF_INET6;
	mask6.sin6_len = sizeof(sa6);
	bcopy(&pr->ndpr_mask, &mask6.sin6_addr, sizeof(struct in6_addr));
	error = rtrequest(RTM_DELETE, (struct sockaddr *)&sa6, NULL,
	    (struct sockaddr *)&mask6, 0, &rt);
	if (error == 0) {
		pr->ndpr_stateflags &= ~NDPRF_ONLINK;

		/* report the route deletion to the routing socket. */
		if (rt != NULL)
			nd6_rtmsg(RTM_DELETE, rt);

		/*
		 * There might be the same prefix on another interface,
		 * the prefix which could not be on-link just because we have
		 * the interface route (see comments in nd6_prefix_onlink).
		 * If there's one, try to make the prefix on-link on the
		 * interface.
		 */
		for (opr = nd_prefix.lh_first; opr; opr = opr->ndpr_next) {
			if (opr == pr)
				continue;

			if ((opr->ndpr_stateflags & NDPRF_ONLINK) != 0)
				continue;

			/*
			 * KAME specific: detached prefixes should not be
			 * on-link.
			 */
			if ((opr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				continue;

			if (opr->ndpr_plen == pr->ndpr_plen &&
			    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
			    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen)) {
				int e;

				if ((e = nd6_prefix_onlink(opr)) != 0) {
					nd6log((LOG_ERR,
					    "nd6_prefix_offlink: failed to "
					    "recover a prefix %s/%d from %s "
					    "to %s (errno = %d)\n",
					    ip6_sprintf(&opr->ndpr_prefix.sin6_addr),
					    opr->ndpr_plen, if_name(ifp),
					    if_name(opr->ndpr_ifp), e));
				}
			}
		}
	} else {
		/* XXX: can we still set the NDPRF_ONLINK flag? */
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: failed to delete route: "
		    "%s/%d on %s (errno = %d)\n",
		    ip6_sprintf(&sa6.sin6_addr), pr->ndpr_plen, if_name(ifp),
		    error));
	}

	if (rt != NULL) {
		RTFREE(rt);
	}

	return (error);
}

static struct in6_ifaddr *
in6_ifadd(pr, ifid)
	struct nd_prefix *pr;
	struct in6_addr  *ifid;   /* Mobile IPv6 addition */
{
	struct ifnet *ifp = pr->ndpr_ifp;
	struct ifaddr *ifa;
	struct in6_aliasreq ifra;
	struct in6_ifaddr *ia, *ib;
	int error, plen0;
	struct in6_addr mask;
	int prefixlen = pr->ndpr_plen;

	in6_prefixlen2mask(&mask, prefixlen);

	/*
	 * find a link-local address (will be interface ID).
	 * Is it really mandatory? Theoretically, a global or a site-local
	 * address can be configured without a link-local address, if we
	 * have a unique interface identifier...
	 *
	 * it is not mandatory to have a link-local address, we can generate
	 * interface identifier on the fly.  we do this because:
	 * (1) it should be the easiest way to find interface identifier.
	 * (2) RFC2462 5.4 suggesting the use of the same interface identifier
	 * for multiple addresses on a single interface, and possible shortcut
	 * of DAD.  we omitted DAD for this reason in the past.
	 * (3) a user can prevent autoconfiguration of global address
	 * by removing link-local address by hand (this is partly because we
	 * don't have other way to control the use of IPv6 on an interface.
	 * this has been our design choice - cf. NRL's "ifconfig auto").
	 * (4) it is easier to manage when an interface has addresses
	 * with the same interface identifier, than to have multiple addresses
	 * with different interface identifiers.
	 */
	ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp, 0); /* 0 is OK? */
	if (ifa)
		ib = (struct in6_ifaddr *)ifa;
	else
		return NULL;

#if 0 /* don't care link local addr state, and always do DAD */
	/* if link-local address is not eligible, do not autoconfigure. */
	if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_NOTREADY) {
		printf("in6_ifadd: link-local address not ready\n");
		return NULL;
	}
#endif

	/* prefixlen + ifidlen must be equal to 128 */
	plen0 = in6_mask2len(&ib->ia_prefixmask.sin6_addr, NULL);
	if (prefixlen != plen0) {
		nd6log((LOG_INFO, "in6_ifadd: wrong prefixlen for %s "
		    "(prefix=%d ifid=%d)\n",
		    if_name(ifp), prefixlen, 128 - plen0));
		return NULL;
	}

	/* make ifaddr */

	bzero(&ifra, sizeof(ifra));
	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));
	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	/* prefix */
	bcopy(&pr->ndpr_prefix.sin6_addr, &ifra.ifra_addr.sin6_addr,
	    sizeof(ifra.ifra_addr.sin6_addr));
	ifra.ifra_addr.sin6_addr.s6_addr32[0] &= mask.s6_addr32[0];
	ifra.ifra_addr.sin6_addr.s6_addr32[1] &= mask.s6_addr32[1];
	ifra.ifra_addr.sin6_addr.s6_addr32[2] &= mask.s6_addr32[2];
	ifra.ifra_addr.sin6_addr.s6_addr32[3] &= mask.s6_addr32[3];

	/* interface ID */
	if (ifid == NULL || IN6_IS_ADDR_UNSPECIFIED(ifid))
		ifid = &ib->ia_addr.sin6_addr;
	ifra.ifra_addr.sin6_addr.s6_addr32[0] |=
	    (ifid->s6_addr32[0] & ~mask.s6_addr32[0]);
	ifra.ifra_addr.sin6_addr.s6_addr32[1] |=
	    (ifid->s6_addr32[1] & ~mask.s6_addr32[1]);
	ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
	    (ifid->s6_addr32[2] & ~mask.s6_addr32[2]);
	ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
	    (ifid->s6_addr32[3] & ~mask.s6_addr32[3]);

	/* new prefix mask. */
	ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_prefixmask.sin6_family = AF_INET6;
	bcopy(&mask, &ifra.ifra_prefixmask.sin6_addr,
	    sizeof(ifra.ifra_prefixmask.sin6_addr));

	/*
	 * lifetime.
	 * XXX: in6_init_address_ltimes would override these values later.
	 * We should reconsider this logic.
	 */
	ifra.ifra_lifetime.ia6t_vltime = pr->ndpr_vltime;
	ifra.ifra_lifetime.ia6t_pltime = pr->ndpr_pltime;

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= IN6_IFF_AUTOCONF; /* obey autoconf */
	/*
	 * temporarily set the nopfx flag to avoid conflict.
	 * XXX: we should reconsider the entire mechanism about prefix
	 * manipulation.
	 */
	ifra.ifra_flags |= IN6_IFF_NOPFX;

	/*
	 * keep the new address, regardless of the result of in6_update_ifa.
	 * XXX: this address is now meaningless.
	 * We should reconsider its role.
	 */
	pr->ndpr_addr = ifra.ifra_addr.sin6_addr;

	/* allocate ifaddr structure, link into chain, etc. */
	if ((error = in6_update_ifa(ifp, &ifra, NULL)) != 0) {
		nd6log((LOG_ERR,
		    "in6_ifadd: failed to make ifaddr %s on %s (errno=%d)\n",
		    ip6_sprintf(&ifra.ifra_addr.sin6_addr), if_name(ifp),
		    error));
		return (NULL);	/* ifaddr must not have been allocated. */
	}

	ia = in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr);

	return (ia);		/* this is always non-NULL */
}

int
in6_tmpifadd(ia0, forcegen)
	const struct in6_ifaddr *ia0; /* corresponding public address */
	int forcegen;
{
	struct ifnet *ifp = ia0->ia_ifa.ifa_ifp;
	struct in6_ifaddr *newia;
	struct in6_aliasreq ifra;
	int i, error;
	int trylimit = 3;	/* XXX: adhoc value */
	u_int32_t randid[2];
	time_t vltime0, pltime0;

	bzero(&ifra, sizeof(ifra));
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));
	ifra.ifra_addr = ia0->ia_addr;
	/* copy prefix mask */
	ifra.ifra_prefixmask = ia0->ia_prefixmask;
	/* clear the old IFID */
	for (i = 0; i < 4; i++) {
		ifra.ifra_addr.sin6_addr.s6_addr32[i] &=
		    ifra.ifra_prefixmask.sin6_addr.s6_addr32[i];
	}

  again:
	in6_get_tmpifid(ifp, (u_int8_t *)randid,
	    (const u_int8_t *)&ia0->ia_addr.sin6_addr.s6_addr[8], forcegen);
	ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
	    (randid[0] & ~(ifra.ifra_prefixmask.sin6_addr.s6_addr32[2]));
	ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
	    (randid[1] & ~(ifra.ifra_prefixmask.sin6_addr.s6_addr32[3]));

	/*
	 * If by chance the new temporary address is the same as an address
	 * already assigned to the interface, generate a new randomized
	 * interface identifier and repeat this step.
	 * RFC 3041 3.3 (4).
	 */
	if (in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr) != NULL) {
		if (trylimit-- == 0) {
			nd6log((LOG_NOTICE, "in6_tmpifadd: failed to find "
			    "a unique random IFID\n"));
			return (EEXIST);
		}
		forcegen = 1;
		goto again;
	}

	/*
	 * The Valid Lifetime is the lower of the Valid Lifetime of the
         * public address or TEMP_VALID_LIFETIME.
	 * The Preferred Lifetime is the lower of the Preferred Lifetime
         * of the public address or TEMP_PREFERRED_LIFETIME -
         * DESYNC_FACTOR.
	 */
	if (ia0->ia6_lifetime.ia6t_expire != 0) {
		vltime0 = IFA6_IS_INVALID(ia0) ? 0 :
		    (ia0->ia6_lifetime.ia6t_expire - time_second);
		if (vltime0 > ip6_temp_valid_lifetime)
			vltime0 = ip6_temp_valid_lifetime;
	} else
		vltime0 = ip6_temp_valid_lifetime;
	if (ia0->ia6_lifetime.ia6t_preferred != 0) {
		pltime0 = IFA6_IS_DEPRECATED(ia0) ? 0 :
		    (ia0->ia6_lifetime.ia6t_preferred - time_second);
		if (pltime0 > ip6_temp_preferred_lifetime - ip6_desync_factor){
			pltime0 = ip6_temp_preferred_lifetime -
			    ip6_desync_factor;
		}
	} else
		pltime0 = ip6_temp_preferred_lifetime - ip6_desync_factor;
	ifra.ifra_lifetime.ia6t_vltime = vltime0;
	ifra.ifra_lifetime.ia6t_pltime = pltime0;

	/*
	 * A temporary address is created only if this calculated Preferred
	 * Lifetime is greater than REGEN_ADVANCE time units.
	 */
	if (ifra.ifra_lifetime.ia6t_pltime <= ip6_temp_regen_advance)
		return (0);

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= (IN6_IFF_AUTOCONF|IN6_IFF_TEMPORARY);

	/* allocate ifaddr structure, link into chain, etc. */
	if ((error = in6_update_ifa(ifp, &ifra, NULL)) != 0)
		return (error);

	newia = in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr);
	if (newia == NULL) {	/* XXX: can it happen? */
		nd6log((LOG_ERR,
		    "in6_tmpifadd: ifa update succeeded, but we got "
		    "no ifaddr\n"));
		return (EINVAL); /* XXX */
	}
	newia->ia6_ndpr = ia0->ia6_ndpr;
	newia->ia6_ndpr->ndpr_refcnt++;

	/*
	 * A newly added address might affect the status of other addresses.
	 * XXX: when the temporary address is generated with a new public
	 * address, the onlink check is redundant.  However, it would be safe
	 * to do the check explicitly everywhere a new address is generated,
	 * and, in fact, we surely need the check when we create a new
	 * temporary address due to deprecation of an old temporary address.
	 */
	pfxlist_onlink_check();

	return (0);
}

int
in6_init_prefix_ltimes(struct nd_prefix *ndpr)
{
	/* check if preferred lifetime > valid lifetime.  RFC2462 5.5.3 (c) */
	if (ndpr->ndpr_pltime > ndpr->ndpr_vltime) {
		nd6log((LOG_INFO, "in6_init_prefix_ltimes: preferred lifetime"
		    "(%d) is greater than valid lifetime(%d)\n",
		    (u_int)ndpr->ndpr_pltime, (u_int)ndpr->ndpr_vltime));
		return (EINVAL);
	}
	if (ndpr->ndpr_pltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_preferred = 0;
	else
		ndpr->ndpr_preferred = time_second + ndpr->ndpr_pltime;
	if (ndpr->ndpr_vltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_expire = 0;
	else
		ndpr->ndpr_expire = time_second + ndpr->ndpr_vltime;

	return 0;
}

static void
in6_init_address_ltimes(struct nd_prefix *new, struct in6_addrlifetime *lt6)
{
	/* init ia6t_expire */
	if (lt6->ia6t_vltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_expire = 0;
	else {
		lt6->ia6t_expire = time_second;
		lt6->ia6t_expire += lt6->ia6t_vltime;
	}

	/* init ia6t_preferred */
	if (lt6->ia6t_pltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_preferred = 0;
	else {
		lt6->ia6t_preferred = time_second;
		lt6->ia6t_preferred += lt6->ia6t_pltime;
	}
}

/*
 * Delete all the routing table entries that use the specified gateway.
 * XXX: this function causes search through all entries of routing table, so
 * it shouldn't be called when acting as a router.
 */
void
rt6_flush(gateway, ifp)
	struct in6_addr *gateway;
	struct ifnet *ifp;
{
	struct radix_node_head *rnh = rt_tables[AF_INET6];
	int s = splnet();

	/* We'll care only link-local addresses */
	if (!IN6_IS_ADDR_LINKLOCAL(gateway)) {
		splx(s);
		return;
	}

	RADIX_NODE_HEAD_LOCK(rnh);
	rnh->rnh_walktree(rnh, rt6_deleteroute, (void *)gateway);
	RADIX_NODE_HEAD_UNLOCK(rnh);
	splx(s);
}

static int
rt6_deleteroute(rn, arg)
	struct radix_node *rn;
	void *arg;
{
#define SIN6(s)	((struct sockaddr_in6 *)s)
	struct rtentry *rt = (struct rtentry *)rn;
	struct in6_addr *gate = (struct in6_addr *)arg;

	if (rt->rt_gateway == NULL || rt->rt_gateway->sa_family != AF_INET6)
		return (0);

	if (!IN6_ARE_ADDR_EQUAL(gate, &SIN6(rt->rt_gateway)->sin6_addr)) {
		return (0);
	}

	/*
	 * Do not delete a static route.
	 * XXX: this seems to be a bit ad-hoc. Should we consider the
	 * 'cloned' bit instead?
	 */
	if ((rt->rt_flags & RTF_STATIC) != 0)
		return (0);

	/*
	 * We delete only host route. This means, in particular, we don't
	 * delete default route.
	 */
	if ((rt->rt_flags & RTF_HOST) == 0)
		return (0);

	return (rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
	    rt_mask(rt), rt->rt_flags, 0));
#undef SIN6
}

int
nd6_setdefaultiface(ifindex)
	int ifindex;
{
	int error = 0;

	if (ifindex < 0 || if_index < ifindex)
		return (EINVAL);

	if (nd6_defifindex != ifindex) {
		nd6_defifindex = ifindex;
		if (nd6_defifindex > 0)
			nd6_defifp = ifnet_byindex(nd6_defifindex);
		else
			nd6_defifp = NULL;

		/*
		 * If the Default Router List is empty, install a route
		 * to the specified interface as default or remove the default
		 * route when the default interface becomes canceled.
		 * The check for the queue is actually redundant, but
		 * we do this here to avoid re-install the default route
		 * if the list is NOT empty.
		 */
		if (TAILQ_FIRST(&nd_defrouter) == NULL)
			defrouter_select();

		/*
		 * Our current implementation assumes one-to-one maping between
		 * interfaces and links, so it would be natural to use the
		 * default interface as the default link.
		 */
		scope6_setdefault(nd6_defifp);
	}

	return (error);
}
