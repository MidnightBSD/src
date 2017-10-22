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
 *	@(#)ipx_input.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netipx/ipx.h>
#include <netipx/spx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>

int	ipxcksum = 0;
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, checksum, CTLFLAG_RW,
	   &ipxcksum, 0, "Compute ipx checksum");

static int	ipxprintfs = 0;		/* printing forwarding information */
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, ipxprintfs, CTLFLAG_RW,
	   &ipxprintfs, 0, "Printing forwarding information");

static int	ipxforwarding = 0;
SYSCTL_INT(_net_ipx_ipx, OID_AUTO, ipxforwarding, CTLFLAG_RW,
	    &ipxforwarding, 0, "Enable ipx forwarding");

static int	ipxnetbios = 0;
SYSCTL_INT(_net_ipx, OID_AUTO, ipxnetbios, CTLFLAG_RW,
	   &ipxnetbios, 0, "Propagate netbios over ipx");

static	int ipx_do_route(struct ipx_addr *src, struct route *ro);
static	void ipx_undo_route(struct route *ro);
static	void ipx_forward(struct mbuf *m);
static	void ipxintr(struct mbuf *m);

const union	ipx_net ipx_zeronet;

const union	ipx_net	ipx_broadnet = { .s_net[0] = 0xffff,
					    .s_net[1] = 0xffff };
const union	ipx_host ipx_broadhost = { .s_host[0] = 0xffff,
					    .s_host[1] = 0xffff,
					    .s_host[2] = 0xffff };

struct	ipxstat ipxstat;
struct	sockaddr_ipx ipx_netmask, ipx_hostmask;

/*
 * IPX protocol control block (pcb) lists.
 */
struct mtx		ipxpcb_list_mtx;
struct ipxpcbhead	ipxpcb_list;
struct ipxpcbhead	ipxrawpcb_list;

static struct netisr_handler ipx_nh = {
	.nh_name = "ipx",
	.nh_handler = ipxintr,
	.nh_proto = NETISR_IPX,
	.nh_policy = NETISR_POLICY_SOURCE,
};

long	ipx_pexseq;		/* Locked with ipxpcb_list_mtx. */

/*
 * IPX initialization.
 */

void
ipx_init(void)
{

	read_random(&ipx_pexseq, sizeof ipx_pexseq);

	LIST_INIT(&ipxpcb_list);
	LIST_INIT(&ipxrawpcb_list);
	TAILQ_INIT(&ipx_ifaddrhead);

	IPX_LIST_LOCK_INIT();
	IPX_IFADDR_LOCK_INIT();

	ipx_netmask.sipx_len = 6;
	ipx_netmask.sipx_addr.x_net = ipx_broadnet;

	ipx_hostmask.sipx_len = 12;
	ipx_hostmask.sipx_addr.x_net = ipx_broadnet;
	ipx_hostmask.sipx_addr.x_host = ipx_broadhost;

	netisr_register(&ipx_nh);
}

/*
 * IPX input routine.  Pass to next level.
 */
static void
ipxintr(struct mbuf *m)
{
	struct ipx *ipx;
	struct ipxpcb *ipxp;
	struct ipx_ifaddr *ia;
	int len;

	/*
	 * If no IPX addresses have been set yet but the interfaces
	 * are receiving, can't do anything with incoming packets yet.
	 */
	if (TAILQ_EMPTY(&ipx_ifaddrhead)) {
		m_freem(m);
		return;
	}

	ipxstat.ipxs_total++;

	if ((m->m_flags & M_EXT || m->m_len < sizeof(struct ipx)) &&
	    (m = m_pullup(m, sizeof(struct ipx))) == NULL) {
		ipxstat.ipxs_toosmall++;
		return;
	}

	/*
	 * Give any raw listeners a crack at the packet
	 */
	IPX_LIST_LOCK();
	LIST_FOREACH(ipxp, &ipxrawpcb_list, ipxp_list) {
		struct mbuf *m1 = m_copy(m, 0, (int)M_COPYALL);
		if (m1 != NULL) {
			IPX_LOCK(ipxp);
			ipx_input(m1, ipxp);
			IPX_UNLOCK(ipxp);
		}
	}
	IPX_LIST_UNLOCK();

	ipx = mtod(m, struct ipx *);
	len = ntohs(ipx->ipx_len);
	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPX header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		ipxstat.ipxs_tooshort++;
		m_freem(m);
		return;
	}
	if (m->m_pkthdr.len > len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else
			m_adj(m, len - m->m_pkthdr.len);
	}
	if (ipxcksum && ipx->ipx_sum != 0xffff) {
		if (ipx->ipx_sum != ipx_cksum(m, len)) {
			ipxstat.ipxs_badsum++;
			m_freem(m);
			return;
		}
	}

	/*
	 * Propagated (Netbios) packets (type 20) has to be handled
	 * different. :-(
	 */
	if (ipx->ipx_pt == IPXPROTO_NETBIOS) {
		if (ipxnetbios) {
			ipx_output_type20(m);
			return;
		} else {
			m_freem(m);
			return;
		}
	}

	/*
	 * Is this a directed broadcast?
	 */
	if (ipx_hosteqnh(ipx_broadhost,ipx->ipx_dna.x_host)) {
		if ((!ipx_neteq(ipx->ipx_dna, ipx->ipx_sna)) &&
		    (!ipx_neteqnn(ipx->ipx_dna.x_net, ipx_broadnet)) &&
		    (!ipx_neteqnn(ipx->ipx_sna.x_net, ipx_zeronet)) &&
		    (!ipx_neteqnn(ipx->ipx_dna.x_net, ipx_zeronet)) ) {
			/*
			 * If it is a broadcast to the net where it was
			 * received from, treat it as ours.
			 */
			IPX_IFADDR_RLOCK();
			TAILQ_FOREACH(ia, &ipx_ifaddrhead, ia_link) {
				if ((ia->ia_ifa.ifa_ifp == m->m_pkthdr.rcvif)
				    && ipx_neteq(ia->ia_addr.sipx_addr,
				    ipx->ipx_dna)) {
					IPX_IFADDR_RUNLOCK();
					goto ours;
				}
			}
			IPX_IFADDR_RUNLOCK();

			/*
			 * Look to see if I need to eat this packet.
			 * Algorithm is to forward all young packets
			 * and prematurely age any packets which will
			 * by physically broadcasted.
			 * Any very old packets eaten without forwarding
			 * would die anyway.
			 *
			 * Suggestion of Bill Nesheim, Cornell U.
			 */
			if (ipx->ipx_tc < IPX_MAXHOPS) {
				ipx_forward(m);
				return;
			}
		}
	/*
	 * Is this our packet? If not, forward.
	 */
	} else {
		IPX_IFADDR_RLOCK();
		TAILQ_FOREACH(ia, &ipx_ifaddrhead, ia_link) {
			if (ipx_hosteq(ipx->ipx_dna, ia->ia_addr.sipx_addr) &&
			    (ipx_neteq(ipx->ipx_dna, ia->ia_addr.sipx_addr) ||
			     ipx_neteqnn(ipx->ipx_dna.x_net, ipx_zeronet)))
				break;
		}
		IPX_IFADDR_RUNLOCK();
		if (ia == NULL) {
			ipx_forward(m);
			return;
		}
	}
ours:
	/*
	 * Locate pcb for datagram.
	 */
	IPX_LIST_LOCK();
	ipxp = ipx_pcblookup(&ipx->ipx_sna, ipx->ipx_dna.x_port, IPX_WILDCARD);
	/*
	 * Switch out to protocol's input routine.
	 */
	if (ipxp != NULL) {
		ipxstat.ipxs_delivered++;
		if ((ipxp->ipxp_flags & IPXP_ALL_PACKETS) == 0)
			switch (ipx->ipx_pt) {
			case IPXPROTO_SPX:
				IPX_LOCK(ipxp);
				/* Will release both locks. */
				spx_input(m, ipxp);
				return;
			}
		IPX_LOCK(ipxp);
		ipx_input(m, ipxp);
		IPX_UNLOCK(ipxp);
	} else
		m_freem(m);
	IPX_LIST_UNLOCK();
}

void
ipx_ctlinput(cmd, arg_as_sa, dummy)
	int cmd;
	struct sockaddr *arg_as_sa;	/* XXX should be swapped with dummy */
	void *dummy;
{

	/* Currently, nothing. */
}

/*
 * Forward a packet. If some error occurs drop the packet. IPX don't
 * have a way to return errors to the sender.
 */

static struct route ipx_droute;
static struct route ipx_sroute;

static void
ipx_forward(struct mbuf *m)
{
	struct ipx *ipx = mtod(m, struct ipx *);
	int error;
	int agedelta = 1;
	int flags = IPX_FORWARDING;
	int ok_there = 0;
	int ok_back = 0;

	if (ipxforwarding == 0) {
		/* can't tell difference between net and host */
		ipxstat.ipxs_cantforward++;
		m_freem(m);
		goto cleanup;
	}
	ipx->ipx_tc++;
	if (ipx->ipx_tc > IPX_MAXHOPS) {
		ipxstat.ipxs_cantforward++;
		m_freem(m);
		goto cleanup;
	}

	if ((ok_there = ipx_do_route(&ipx->ipx_dna,&ipx_droute)) == 0) {
		ipxstat.ipxs_noroute++;
		m_freem(m);
		goto cleanup;
	}
	/*
	 * Here we think about  forwarding  broadcast packets,
	 * so we try to insure that it doesn't go back out
	 * on the interface it came in on.  Also, if we
	 * are going to physically broadcast this, let us
	 * age the packet so we can eat it safely the second time around.
	 */
	if (ipx->ipx_dna.x_host.c_host[0] & 0x1) {
		struct ipx_ifaddr *ia;
		struct ifnet *ifp;

		IPX_IFADDR_RLOCK();
		ia = ipx_iaonnetof(&ipx->ipx_dna);
		if (ia != NULL) {
			/* I'm gonna hafta eat this packet */
			agedelta += IPX_MAXHOPS - ipx->ipx_tc;
			ipx->ipx_tc = IPX_MAXHOPS;
		}
		IPX_IFADDR_RUNLOCK();
		if ((ok_back = ipx_do_route(&ipx->ipx_sna,&ipx_sroute)) == 0) {
			/* error = ENETUNREACH; He'll never get it! */
			ipxstat.ipxs_noroute++;
			m_freem(m);
			goto cleanup;
		}
		if (ipx_droute.ro_rt &&
		    (ifp = ipx_droute.ro_rt->rt_ifp) &&
		    ipx_sroute.ro_rt &&
		    (ifp != ipx_sroute.ro_rt->rt_ifp)) {
			flags |= IPX_ALLOWBROADCAST;
		} else {
			ipxstat.ipxs_noroute++;
			m_freem(m);
			goto cleanup;
		}
	}
	/*
	 * We don't need to recompute checksum because ipx_tc field
	 * is ignored by checksum calculation routine, however
	 * it may be desirable to reset checksum if ipxcksum == 0
	 */
#if 0
	if (!ipxcksum)
		ipx->ipx_sum = 0xffff;
#endif

	error = ipx_outputfl(m, &ipx_droute, flags);
	if (error == 0) {
		ipxstat.ipxs_forward++;

		if (ipxprintfs) {
			printf("forward: ");
			ipx_printhost(&ipx->ipx_sna);
			printf(" to ");
			ipx_printhost(&ipx->ipx_dna);
			printf(" hops %d\n", ipx->ipx_tc);
		}
	} 
cleanup:
	if (ok_there)
		ipx_undo_route(&ipx_droute);
	if (ok_back)
		ipx_undo_route(&ipx_sroute);
}

static int
ipx_do_route(struct ipx_addr *src, struct route *ro)
{
	struct sockaddr_ipx *dst;

	bzero((caddr_t)ro, sizeof(*ro));
	dst = (struct sockaddr_ipx *)&ro->ro_dst;

	dst->sipx_len = sizeof(*dst);
	dst->sipx_family = AF_IPX;
	dst->sipx_addr = *src;
	dst->sipx_addr.x_port = 0;
	rtalloc_ign(ro, 0);
	if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL) {
		return (0);
	}
	ro->ro_rt->rt_use++;
	return (1);
}

static void
ipx_undo_route(struct route *ro)
{

	if (ro->ro_rt != NULL) {
		RTFREE(ro->ro_rt);
	}
}
