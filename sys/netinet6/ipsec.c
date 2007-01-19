/*	$FreeBSD: src/sys/netinet6/ipsec.c,v 1.42.2.2 2006/04/01 15:16:14 rwatson Exp $	*/
/*	$KAME: ipsec.c,v 1.207 2004/01/13 03:30:42 itojun Exp $	*/

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

/*
 * IPsec controller part.
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
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_ecn.h>
#ifdef INET6
#include <netinet6/ip6_ecn.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet/icmp6.h>
#endif

#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netinet6/ah.h>
#ifdef INET6
#include <netinet6/ah6.h>
#endif
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#ifdef INET6
#include <netinet6/esp6.h>
#endif
#endif
#include <netinet6/ipcomp.h>
#ifdef INET6
#include <netinet6/ipcomp6.h>
#endif
#include <netkey/key.h>
#include <netkey/keydb.h>
#include <netkey/key_debug.h>

#include <machine/in_cksum.h>
#include <net/net_osdep.h>

#ifdef IPSEC_DEBUG
int ipsec_debug = 1;
#else
int ipsec_debug = 0;
#endif

NET_NEEDS_GIANT("ipsec");

struct ipsecstat ipsecstat;
int ip4_ah_cleartos = 1;
int ip4_ah_offsetmask = 0;	/* maybe IP_DF? */
int ip4_ipsec_dfbit = 0;	/* DF bit on encap. 0: clear 1: set 2: copy */
int ip4_esp_trans_deflev = IPSEC_LEVEL_USE;
int ip4_esp_net_deflev = IPSEC_LEVEL_USE;
int ip4_ah_trans_deflev = IPSEC_LEVEL_USE;
int ip4_ah_net_deflev = IPSEC_LEVEL_USE;
struct secpolicy *ip4_def_policy;
int ip4_ipsec_ecn = 0;		/* ECN ignore(-1)/forbidden(0)/allowed(1) */
int ip4_esp_randpad = -1;

static int sp_cachegen = 1;	/* cache generation # */

SYSCTL_DECL(_net_inet_ipsec);
#ifdef INET6
SYSCTL_DECL(_net_inet6_ipsec6);
#endif

/* net.inet.ipsec */
SYSCTL_STRUCT(_net_inet_ipsec, IPSECCTL_STATS,
	stats, CTLFLAG_RD,	&ipsecstat,	ipsecstat, "");
#if 0
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_POLICY,
	def_policy, CTLFLAG_RW,	&ip4_def_policy->policy,	0, "");
#endif
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_ESP_TRANSLEV, esp_trans_deflev,
	CTLFLAG_RW, &ip4_esp_trans_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_ESP_NETLEV, esp_net_deflev,
	CTLFLAG_RW, &ip4_esp_net_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_AH_TRANSLEV, ah_trans_deflev,
	CTLFLAG_RW, &ip4_ah_trans_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_AH_NETLEV, ah_net_deflev,
	CTLFLAG_RW, &ip4_ah_net_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_AH_CLEARTOS,
	ah_cleartos, CTLFLAG_RW,	&ip4_ah_cleartos,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_AH_OFFSETMASK,
	ah_offsetmask, CTLFLAG_RW,	&ip4_ah_offsetmask,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DFBIT,
	dfbit, CTLFLAG_RW,	&ip4_ipsec_dfbit,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_ECN,
	ecn, CTLFLAG_RW,	&ip4_ipsec_ecn,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEBUG,
	debug, CTLFLAG_RW,	&ipsec_debug,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_ESP_RANDPAD,
	esp_randpad, CTLFLAG_RW,	&ip4_esp_randpad,	0, "");

#ifdef INET6
struct ipsecstat ipsec6stat;
int ip6_esp_trans_deflev = IPSEC_LEVEL_USE;
int ip6_esp_net_deflev = IPSEC_LEVEL_USE;
int ip6_ah_trans_deflev = IPSEC_LEVEL_USE;
int ip6_ah_net_deflev = IPSEC_LEVEL_USE;
struct secpolicy *ip6_def_policy;
int ip6_ipsec_ecn = 0;		/* ECN ignore(-1)/forbidden(0)/allowed(1) */
int ip6_esp_randpad = -1;

/* net.inet6.ipsec6 */
SYSCTL_STRUCT(_net_inet6_ipsec6, IPSECCTL_STATS,
	stats, CTLFLAG_RD, &ipsec6stat, ipsecstat, "");
#if 0
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_POLICY,
	def_policy, CTLFLAG_RW,	&ip6_def_policy->policy,	0, "");
#endif
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_ESP_TRANSLEV, esp_trans_deflev,
	CTLFLAG_RW, &ip6_esp_trans_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_ESP_NETLEV, esp_net_deflev,
	CTLFLAG_RW, &ip6_esp_net_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_AH_TRANSLEV, ah_trans_deflev,
	CTLFLAG_RW, &ip6_ah_trans_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_AH_NETLEV, ah_net_deflev,
	CTLFLAG_RW, &ip6_ah_net_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_ECN,
	ecn, CTLFLAG_RW,	&ip6_ipsec_ecn,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEBUG,
	debug, CTLFLAG_RW,	&ipsec_debug,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_ESP_RANDPAD,
	esp_randpad, CTLFLAG_RW,	&ip6_esp_randpad,	0, "");
#endif /* INET6 */

static struct secpolicy *ipsec_checkpcbcache __P((struct mbuf *,
	struct inpcbpolicy *, int));
static int ipsec_fillpcbcache __P((struct inpcbpolicy *, struct mbuf *,
	struct secpolicy *, int));
static int ipsec_invalpcbcache __P((struct inpcbpolicy *, int));
static int ipsec_setspidx_mbuf
	__P((struct secpolicyindex *, int, struct mbuf *, int));
static int ipsec_setspidx __P((struct mbuf *, struct secpolicyindex *, int));
static void ipsec4_get_ulp __P((struct mbuf *, struct secpolicyindex *, int));
static int ipsec4_setspidx_ipaddr __P((struct mbuf *, struct secpolicyindex *));
#ifdef INET6
static void ipsec6_get_ulp __P((struct mbuf *, struct secpolicyindex *, int));
static int ipsec6_setspidx_ipaddr __P((struct mbuf *, struct secpolicyindex *));
#endif
static struct inpcbpolicy *ipsec_newpcbpolicy __P((void));
static void ipsec_delpcbpolicy __P((struct inpcbpolicy *));
#if 0
static int ipsec_deepcopy_pcbpolicy __P((struct inpcbpolicy *));
#endif
static struct secpolicy *ipsec_deepcopy_policy __P((struct secpolicy *));
static int ipsec_set_policy
	__P((struct secpolicy **, int, caddr_t, size_t, int));
static int ipsec_get_policy __P((struct secpolicy *, struct mbuf **));
static void vshiftl __P((unsigned char *, int, int));
static int ipsec_in_reject __P((struct secpolicy *, struct mbuf *));
static size_t ipsec_hdrsiz __P((struct secpolicy *));
#ifdef INET
static struct mbuf *ipsec4_splithdr __P((struct mbuf *));
#endif
#ifdef INET6
static struct mbuf *ipsec6_splithdr __P((struct mbuf *));
#endif
#ifdef INET
static int ipsec4_encapsulate __P((struct mbuf *, struct secasvar *));
#endif
#ifdef INET6
static int ipsec6_encapsulate __P((struct mbuf *, struct secasvar *));
#endif
static struct ipsecaux *ipsec_addaux __P((struct mbuf *));
static struct ipsecaux *ipsec_findaux __P((struct mbuf *));
static void ipsec_optaux __P((struct mbuf *, struct ipsecaux *));
#ifdef INET
static int ipsec4_checksa __P((struct ipsecrequest *,
	struct ipsec_output_state *));
#endif
#ifdef INET6
static int ipsec6_checksa __P((struct ipsecrequest *,
	struct ipsec_output_state *, int));
#endif

/*
 * try to validate and use cached policy on a pcb.
 */
static struct secpolicy *
ipsec_checkpcbcache(m, pcbsp, dir)
	struct mbuf *m;
	struct inpcbpolicy *pcbsp;
	int dir;
{
	struct secpolicyindex spidx;
	struct timeval mono_time;

	microtime(&mono_time);

	switch (dir) {
	case IPSEC_DIR_INBOUND:
	case IPSEC_DIR_OUTBOUND:
	case IPSEC_DIR_ANY:
		break;
	default:
		return NULL;
	}
#ifdef DIAGNOSTIC
	if (dir >= sizeof(pcbsp->cache)/sizeof(pcbsp->cache[0]))
		panic("dir too big in ipsec_checkpcbcache");
#endif
	/* SPD table change invalidates all the caches */
	if (pcbsp->cachegen[dir] == 0 || sp_cachegen > pcbsp->cachegen[dir]) {
		ipsec_invalpcbcache(pcbsp, dir);
		return NULL;
	}
	if (!pcbsp->cache[dir])
		return NULL;
	if (pcbsp->cache[dir]->state != IPSEC_SPSTATE_ALIVE) {
		ipsec_invalpcbcache(pcbsp, dir);
		return NULL;
	}
	if ((pcbsp->cacheflags & IPSEC_PCBSP_CONNECTED) == 0) {
		if (!pcbsp->cache[dir])
			return NULL;
		if (ipsec_setspidx(m, &spidx, 1) != 0)
			return NULL;
		if (bcmp(&pcbsp->cacheidx[dir], &spidx, sizeof(spidx))) {
			if (!pcbsp->cache[dir]->spidx ||
			    !key_cmpspidx_withmask(pcbsp->cache[dir]->spidx,
			    &spidx))
				return NULL;
			pcbsp->cacheidx[dir] = spidx;
		}
	} else {
		/*
		 * The pcb is connected, and the L4 code is sure that:
		 * - outgoing side uses inp_[lf]addr
		 * - incoming side looks up policy after inpcb lookup
		 * and address pair is known to be stable.  We do not need
		 * to generate spidx again, nor check the address match again.
		 *
		 * For IPv4/v6 SOCK_STREAM sockets, this assumption holds
		 * and there are calls to ipsec_pcbconn() from in_pcbconnect().
		 */
	}

	pcbsp->cache[dir]->lastused = mono_time.tv_sec;
	pcbsp->cache[dir]->refcnt++;
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP ipsec_checkpcbcache cause refcnt++:%d SP:%p\n",
		pcbsp->cache[dir]->refcnt, pcbsp->cache[dir]));
	return pcbsp->cache[dir];
}

static int
ipsec_fillpcbcache(pcbsp, m, sp, dir)
	struct inpcbpolicy *pcbsp;
	struct mbuf *m;
	struct secpolicy *sp;
	int dir;
{

	switch (dir) {
	case IPSEC_DIR_INBOUND:
	case IPSEC_DIR_OUTBOUND:
		break;
	default:
		return EINVAL;
	}
#ifdef DIAGNOSTIC
	if (dir >= sizeof(pcbsp->cache)/sizeof(pcbsp->cache[0]))
		panic("dir too big in ipsec_checkpcbcache");
#endif

	if (pcbsp->cache[dir])
		key_freesp(pcbsp->cache[dir]);
	pcbsp->cache[dir] = NULL;
	if (ipsec_setspidx(m, &pcbsp->cacheidx[dir], 1) != 0) {
		return EINVAL;
	}
	pcbsp->cache[dir] = sp;
	if (pcbsp->cache[dir]) {
		pcbsp->cache[dir]->refcnt++;
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ipsec_fillpcbcache cause refcnt++:%d SP:%p\n",
			pcbsp->cache[dir]->refcnt, pcbsp->cache[dir]));
	}
	pcbsp->cachegen[dir] = sp_cachegen;

	return 0;
}

static int
ipsec_invalpcbcache(pcbsp, dir)
	struct inpcbpolicy *pcbsp;
	int dir;
{
	int i;

	for (i = IPSEC_DIR_INBOUND; i <= IPSEC_DIR_OUTBOUND; i++) {
		if (dir != IPSEC_DIR_ANY && i != dir)
			continue;
		if (pcbsp->cache[i])
			key_freesp(pcbsp->cache[i]);
		pcbsp->cache[i] = NULL;
		pcbsp->cachegen[i] = 0;
		bzero(&pcbsp->cacheidx[i], sizeof(pcbsp->cacheidx[i]));
	}
	return 0;
}

int
ipsec_pcbconn(pcbsp)
	struct inpcbpolicy *pcbsp;
{

	pcbsp->cacheflags |= IPSEC_PCBSP_CONNECTED;
	ipsec_invalpcbcache(pcbsp, IPSEC_DIR_ANY);
	return 0;
}

int
ipsec_pcbdisconn(pcbsp)
	struct inpcbpolicy *pcbsp;
{

	pcbsp->cacheflags &= ~IPSEC_PCBSP_CONNECTED;
	ipsec_invalpcbcache(pcbsp, IPSEC_DIR_ANY);
	return 0;
}

int
ipsec_invalpcbcacheall()
{

	sp_cachegen++;
	return 0;
}

/*
 * For OUTBOUND packet having a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 *	others:	a pointer to SP
 *
 * NOTE: IPv6 mapped adddress concern is implemented here.
 */
struct secpolicy *
ipsec4_getpolicybypcb(m, dir, inp, error)
	struct mbuf *m;
	u_int dir;
	struct inpcb *inp;
	int *error;
{
	struct inpcbpolicy *pcbsp = NULL;
	struct secpolicy *currsp = NULL;	/* policy on socket */
	struct secpolicy *kernsp = NULL;	/* policy on kernel */
	struct secpolicyindex spidx;
	u_int16_t tag;

	/* sanity check */
	if (m == NULL || inp == NULL || error == NULL)
		panic("ipsec4_getpolicybypcb: NULL pointer was passed.");

	pcbsp = inp->inp_sp;

#ifdef DIAGNOSTIC
	if (pcbsp == NULL)
		panic("ipsec4_getpolicybypcb: pcbsp is NULL.");
#endif

	tag = 0;

	/* if we have a cached entry, and if it is still valid, use it. */
	ipsecstat.spdcachelookup++;
	currsp = ipsec_checkpcbcache(m, pcbsp, dir);
	if (currsp) {
		*error = 0;
		return currsp;
	}
	ipsecstat.spdcachemiss++;

	switch (dir) {
	case IPSEC_DIR_INBOUND:
		currsp = pcbsp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		currsp = pcbsp->sp_out;
		break;
	default:
		panic("ipsec4_getpolicybypcb: illegal direction.");
	}

	/* sanity check */
	if (currsp == NULL)
		panic("ipsec4_getpolicybypcb: currsp is NULL.");

	/* when privileged socket */
	if (pcbsp->priv) {
		switch (currsp->policy) {
		case IPSEC_POLICY_BYPASS:
			currsp->refcnt++;
			*error = 0;
			ipsec_fillpcbcache(pcbsp, m, currsp, dir);
			return currsp;

		case IPSEC_POLICY_ENTRUST:
			/* look for a policy in SPD */
			if (ipsec_setspidx_mbuf(&spidx, AF_INET, m, 1) == 0 &&
			    (kernsp = key_allocsp(tag, &spidx, dir)) != NULL) {
				/* SP found */
				KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
					printf("DP ipsec4_getpolicybypcb called "
					       "to allocate SP:%p\n", kernsp));
				*error = 0;
				ipsec_fillpcbcache(pcbsp, m, kernsp, dir);
				return kernsp;
			}

			/* no SP found */
			ip4_def_policy->refcnt++;
			*error = 0;
			ipsec_fillpcbcache(pcbsp, m, ip4_def_policy, dir);
			return ip4_def_policy;

		case IPSEC_POLICY_IPSEC:
			currsp->refcnt++;
			*error = 0;
			ipsec_fillpcbcache(pcbsp, m, currsp, dir);
			return currsp;

		default:
			ipseclog((LOG_ERR, "ipsec4_getpolicybypcb: "
			      "Invalid policy for PCB %d\n", currsp->policy));
			*error = EINVAL;
			return NULL;
		}
		/* NOTREACHED */
	}

	/* when non-privileged socket */
	/* look for a policy in SPD */
	if (ipsec_setspidx_mbuf(&spidx, AF_INET, m, 1) == 0 &&
	    (kernsp = key_allocsp(tag, &spidx, dir)) != NULL) {
		/* SP found */
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ipsec4_getpolicybypcb called "
			       "to allocate SP:%p\n", kernsp));
		*error = 0;
		ipsec_fillpcbcache(pcbsp, m, kernsp, dir);
		return kernsp;
	}

	/* no SP found */
	switch (currsp->policy) {
	case IPSEC_POLICY_BYPASS:
		ipseclog((LOG_ERR, "ipsec4_getpolicybypcb: "
		       "Illegal policy for non-privileged defined %d\n",
			currsp->policy));
		*error = EINVAL;
		return NULL;

	case IPSEC_POLICY_ENTRUST:
		ip4_def_policy->refcnt++;
		*error = 0;
		ipsec_fillpcbcache(pcbsp, m, ip4_def_policy, dir);
		return ip4_def_policy;

	case IPSEC_POLICY_IPSEC:
		currsp->refcnt++;
		*error = 0;
		ipsec_fillpcbcache(pcbsp, m, currsp, dir);
		return currsp;

	default:
		ipseclog((LOG_ERR, "ipsec4_getpolicybypcb: "
		   "Invalid policy for PCB %d\n", currsp->policy));
		*error = EINVAL;
		return NULL;
	}
	/* NOTREACHED */
}

/*
 * For FORWADING packet or OUTBOUND without a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	positive: a pointer to the entry for security policy leaf matched.
 *	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 */
struct secpolicy *
ipsec4_getpolicybyaddr(m, dir, flag, error)
	struct mbuf *m;
	u_int dir;
	int flag;
	int *error;
{
	struct secpolicy *sp = NULL;
	u_int16_t tag;

	/* sanity check */
	if (m == NULL || error == NULL)
		panic("ipsec4_getpolicybyaddr: NULL pointer was passed.");

	/* get a policy entry matched with the packet */
    {
	struct secpolicyindex spidx;

	bzero(&spidx, sizeof(spidx));

	/* make an index to look for a policy */
	*error = ipsec_setspidx_mbuf(&spidx, AF_INET, m,
	    (flag & IP_FORWARDING) ? 0 : 1);

	if (*error != 0)
		return NULL;

	tag = 0;

	sp = key_allocsp(tag, &spidx, dir);
    }

	/* SP found */
	if (sp != NULL) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ipsec4_getpolicybyaddr called "
			       "to allocate SP:%p\n", sp));
		*error = 0;
		return sp;
	}

	/* no SP found */
	ip4_def_policy->refcnt++;
	*error = 0;
	return ip4_def_policy;
}

#ifdef INET6
/*
 * For OUTBOUND packet having a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 *	others:	a pointer to SP
 */
struct secpolicy *
ipsec6_getpolicybypcb(m, dir, inp, error)
	struct mbuf *m;
	u_int dir;
	struct inpcb *inp;
	int *error;
{
	struct inpcbpolicy *pcbsp = NULL;
	struct secpolicy *currsp = NULL;	/* policy on socket */
	struct secpolicy *kernsp = NULL;	/* policy on kernel */
	struct secpolicyindex spidx;
	u_int16_t tag;

	/* sanity check */
	if (m == NULL || inp == NULL || error == NULL)
		panic("ipsec6_getpolicybypcb: NULL pointer was passed.");

#ifdef DIAGNOSTIC
	if ((inp->inp_vflag & INP_IPV6PROTO) == 0)
		panic("ipsec6_getpolicybypcb: socket domain != inet6");
#endif

	pcbsp = inp->in6p_sp;

#ifdef DIAGNOSTIC
	if (pcbsp == NULL)
		panic("ipsec6_getpolicybypcb: pcbsp is NULL.");
#endif

	tag = 0;

	/* if we have a cached entry, and if it is still valid, use it. */
	ipsec6stat.spdcachelookup++;
	currsp = ipsec_checkpcbcache(m, pcbsp, dir);
	if (currsp) {
		*error = 0;
		return currsp;
	}
	ipsec6stat.spdcachemiss++;

	switch (dir) {
	case IPSEC_DIR_INBOUND:
		currsp = pcbsp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		currsp = pcbsp->sp_out;
		break;
	default:
		panic("ipsec6_getpolicybypcb: illegal direction.");
	}

	/* sanity check */
	if (currsp == NULL)
		panic("ipsec6_getpolicybypcb: currsp is NULL.");

	/* when privileged socket */
	if (pcbsp->priv) {
		switch (currsp->policy) {
		case IPSEC_POLICY_BYPASS:
			currsp->refcnt++;
			*error = 0;
			ipsec_fillpcbcache(pcbsp, m, currsp, dir);
			return currsp;

		case IPSEC_POLICY_ENTRUST:
			/* look for a policy in SPD */
			if (ipsec_setspidx_mbuf(&spidx, AF_INET6, m, 1) == 0 &&
			    (kernsp = key_allocsp(tag, &spidx, dir)) != NULL) {
				/* SP found */
				KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
					printf("DP ipsec6_getpolicybypcb called "
					       "to allocate SP:%p\n", kernsp));
				*error = 0;
				ipsec_fillpcbcache(pcbsp, m, kernsp, dir);
				return kernsp;
			}

			/* no SP found */
			ip6_def_policy->refcnt++;
			*error = 0;
			ipsec_fillpcbcache(pcbsp, m, ip6_def_policy, dir);
			return ip6_def_policy;

		case IPSEC_POLICY_IPSEC:
			currsp->refcnt++;
			*error = 0;
			ipsec_fillpcbcache(pcbsp, m, currsp, dir);
			return currsp;

		default:
			ipseclog((LOG_ERR, "ipsec6_getpolicybypcb: "
			    "Invalid policy for PCB %d\n", currsp->policy));
			*error = EINVAL;
			return NULL;
		}
		/* NOTREACHED */
	}

	/* when non-privileged socket */
	/* look for a policy in SPD */
	if (ipsec_setspidx_mbuf(&spidx, AF_INET6, m, 1) == 0 &&
	    (kernsp = key_allocsp(tag, &spidx, dir)) != NULL) {
		/* SP found */
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ipsec6_getpolicybypcb called "
			       "to allocate SP:%p\n", kernsp));
		*error = 0;
		ipsec_fillpcbcache(pcbsp, m, kernsp, dir);
		return kernsp;
	}

	/* no SP found */
	switch (currsp->policy) {
	case IPSEC_POLICY_BYPASS:
		ipseclog((LOG_ERR, "ipsec6_getpolicybypcb: "
		    "Illegal policy for non-privileged defined %d\n",
		    currsp->policy));
		*error = EINVAL;
		return NULL;

	case IPSEC_POLICY_ENTRUST:
		ip6_def_policy->refcnt++;
		*error = 0;
		ipsec_fillpcbcache(pcbsp, m, ip6_def_policy, dir);
		return ip6_def_policy;

	case IPSEC_POLICY_IPSEC:
		currsp->refcnt++;
		*error = 0;
		ipsec_fillpcbcache(pcbsp, m, currsp, dir);
		return currsp;

	default:
		ipseclog((LOG_ERR,
		    "ipsec6_policybysock: Invalid policy for PCB %d\n",
		    currsp->policy));
		*error = EINVAL;
		return NULL;
	}
	/* NOTREACHED */
}

/*
 * For FORWADING packet or OUTBOUND without a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * `flag' means that packet is to be forwarded whether or not.
 *	flag = 1: forwad
 * OUT:	positive: a pointer to the entry for security policy leaf matched.
 *	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 */
#ifndef IP_FORWARDING
#define IP_FORWARDING 1
#endif

struct secpolicy *
ipsec6_getpolicybyaddr(m, dir, flag, error)
	struct mbuf *m;
	u_int dir;
	int flag;
	int *error;
{
	struct secpolicy *sp = NULL;
	u_int16_t tag;

	/* sanity check */
	if (m == NULL || error == NULL)
		panic("ipsec6_getpolicybyaddr: NULL pointer was passed.");

	/* get a policy entry matched with the packet */
    {
	struct secpolicyindex spidx;

	bzero(&spidx, sizeof(spidx));

	/* make an index to look for a policy */
	*error = ipsec_setspidx_mbuf(&spidx, AF_INET6, m,
	    (flag & IP_FORWARDING) ? 0 : 1);

	if (*error != 0)
		return NULL;

	tag = 0;

	sp = key_allocsp(tag, &spidx, dir);
    }

	/* SP found */
	if (sp != NULL) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ipsec6_getpolicybyaddr called "
			       "to allocate SP:%p\n", sp));
		*error = 0;
		return sp;
	}

	/* no SP found */
	ip6_def_policy->refcnt++;
	*error = 0;
	return ip6_def_policy;
}
#endif /* INET6 */

/*
 * set IP address into spidx from mbuf.
 * When Forwarding packet and ICMP echo reply, this function is used.
 *
 * IN:	get the followings from mbuf.
 *	protocol family, src, dst, next protocol
 * OUT:
 *	0:	success.
 *	other:	failure, and set errno.
 */
int
ipsec_setspidx_mbuf(spidx, family, m, needport)
	struct secpolicyindex *spidx;
	int family;
	struct mbuf *m;
	int needport;
{
	int error;

	/* sanity check */
	if (spidx == NULL || m == NULL)
		panic("ipsec_setspidx_mbuf: NULL pointer was passed.");

	bzero(spidx, sizeof(*spidx));

	error = ipsec_setspidx(m, spidx, needport);
	if (error)
		goto bad;

	return 0;

    bad:
	/* XXX initialize */
	bzero(spidx, sizeof(*spidx));
	return EINVAL;
}

/*
 * configure security policy index (src/dst/proto/sport/dport)
 * by looking at the content of mbuf.
 * the caller is responsible for error recovery (like clearing up spidx).
 */
static int
ipsec_setspidx(m, spidx, needport)
	struct mbuf *m;
	struct secpolicyindex *spidx;
	int needport;
{
	struct ip *ip = NULL;
	struct ip ipbuf;
	u_int v;
	struct mbuf *n;
	int len;
	int error;

	if (m == NULL)
		panic("ipsec_setspidx: m == 0 passed.");

	bzero(spidx, sizeof(*spidx));

	/*
	 * validate m->m_pkthdr.len.  we see incorrect length if we
	 * mistakenly call this function with inconsistent mbuf chain
	 * (like 4.4BSD tcp/udp processing).  XXX should we panic here?
	 */
	len = 0;
	for (n = m; n; n = n->m_next)
		len += n->m_len;
	if (m->m_pkthdr.len != len) {
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("ipsec_setspidx: "
			       "total of m_len(%d) != pkthdr.len(%d), "
			       "ignored.\n",
				len, m->m_pkthdr.len));
		return EINVAL;
	}

	if (m->m_pkthdr.len < sizeof(struct ip)) {
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("ipsec_setspidx: "
			    "pkthdr.len(%d) < sizeof(struct ip), ignored.\n",
			    m->m_pkthdr.len));
		return EINVAL;
	}

	if (m->m_len >= sizeof(*ip))
		ip = mtod(m, struct ip *);
	else {
		m_copydata(m, 0, sizeof(ipbuf), (caddr_t)&ipbuf);
		ip = &ipbuf;
	}
#ifdef _IP_VHL
	v = _IP_VHL_V(ip->ip_vhl);
#else
	v = ip->ip_v;
#endif
	switch (v) {
	case 4:
		error = ipsec4_setspidx_ipaddr(m, spidx);
		if (error)
			return error;
		ipsec4_get_ulp(m, spidx, needport);
		return 0;
#ifdef INET6
	case 6:
		if (m->m_pkthdr.len < sizeof(struct ip6_hdr)) {
			KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				printf("ipsec_setspidx: "
				    "pkthdr.len(%d) < sizeof(struct ip6_hdr), "
				    "ignored.\n", m->m_pkthdr.len));
			return EINVAL;
		}
		error = ipsec6_setspidx_ipaddr(m, spidx);
		if (error)
			return error;
		ipsec6_get_ulp(m, spidx, needport);
		return 0;
#endif
	default:
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("ipsec_setspidx: "
			    "unknown IP version %u, ignored.\n", v));
		return EINVAL;
	}
}

static void
ipsec4_get_ulp(m, spidx, needport)
	struct mbuf *m;
	struct secpolicyindex *spidx;
	int needport;
{
	struct ip ip;
	struct ip6_ext ip6e;
	u_int8_t nxt;
	int off;
	struct tcphdr th;
	struct udphdr uh;

	/* sanity check */
	if (m == NULL)
		panic("ipsec4_get_ulp: NULL pointer was passed.");
	if (m->m_pkthdr.len < sizeof(ip))
		panic("ipsec4_get_ulp: too short");

	/* set default */
	spidx->ul_proto = IPSEC_ULPROTO_ANY;
	((struct sockaddr_in *)&spidx->src)->sin_port = IPSEC_PORT_ANY;
	((struct sockaddr_in *)&spidx->dst)->sin_port = IPSEC_PORT_ANY;

	m_copydata(m, 0, sizeof(ip), (caddr_t)&ip);
	/* ip_input() flips it into host endian XXX need more checking */
	if (ip.ip_off & (IP_MF | IP_OFFMASK))
		return;

	nxt = ip.ip_p;
#ifdef _IP_VHL
	off = _IP_VHL_HL(ip->ip_vhl) << 2;
#else
	off = ip.ip_hl << 2;
#endif
	while (off < m->m_pkthdr.len) {
		switch (nxt) {
		case IPPROTO_TCP:
			spidx->ul_proto = nxt;
			if (!needport)
				return;
			if (off + sizeof(struct tcphdr) > m->m_pkthdr.len)
				return;
			m_copydata(m, off, sizeof(th), (caddr_t)&th);
			((struct sockaddr_in *)&spidx->src)->sin_port =
			    th.th_sport;
			((struct sockaddr_in *)&spidx->dst)->sin_port =
			    th.th_dport;
			return;
		case IPPROTO_UDP:
			spidx->ul_proto = nxt;
			if (!needport)
				return;
			if (off + sizeof(struct udphdr) > m->m_pkthdr.len)
				return;
			m_copydata(m, off, sizeof(uh), (caddr_t)&uh);
			((struct sockaddr_in *)&spidx->src)->sin_port =
			    uh.uh_sport;
			((struct sockaddr_in *)&spidx->dst)->sin_port =
			    uh.uh_dport;
			return;
		case IPPROTO_AH:
			if (off + sizeof(ip6e) > m->m_pkthdr.len)
				return;
			m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
			off += (ip6e.ip6e_len + 2) << 2;
			nxt = ip6e.ip6e_nxt;
			break;
		case IPPROTO_ICMP:
		default:
			/* XXX intermediate headers??? */
			spidx->ul_proto = nxt;
			return;
		}
	}
}

/* assumes that m is sane */
static int
ipsec4_setspidx_ipaddr(m, spidx)
	struct mbuf *m;
	struct secpolicyindex *spidx;
{
	struct ip *ip = NULL;
	struct ip ipbuf;
	struct sockaddr_in *sin;

	if (m->m_len >= sizeof(*ip))
		ip = mtod(m, struct ip *);
	else {
		m_copydata(m, 0, sizeof(ipbuf), (caddr_t)&ipbuf);
		ip = &ipbuf;
	}

	sin = (struct sockaddr_in *)&spidx->src;
	bzero(sin, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	bcopy(&ip->ip_src, &sin->sin_addr, sizeof(ip->ip_src));
	spidx->prefs = sizeof(struct in_addr) << 3;

	sin = (struct sockaddr_in *)&spidx->dst;
	bzero(sin, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	bcopy(&ip->ip_dst, &sin->sin_addr, sizeof(ip->ip_dst));
	spidx->prefd = sizeof(struct in_addr) << 3;
	return 0;
}

#ifdef INET6
static void
ipsec6_get_ulp(m, spidx, needport)
	struct mbuf *m;
	struct secpolicyindex *spidx;
	int needport;
{
	int off, nxt;
	struct tcphdr th;
	struct udphdr uh;
	struct icmp6_hdr ih;

	/* sanity check */
	if (m == NULL)
		panic("ipsec6_get_ulp: NULL pointer was passed.");

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec6_get_ulp:\n"); kdebug_mbuf(m));

	/* set default */
	spidx->ul_proto = IPSEC_ULPROTO_ANY;
	((struct sockaddr_in6 *)&spidx->src)->sin6_port = IPSEC_PORT_ANY;
	((struct sockaddr_in6 *)&spidx->dst)->sin6_port = IPSEC_PORT_ANY;

	nxt = -1;
	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off < 0 || m->m_pkthdr.len < off)
		return;

	switch (nxt) {
	case IPPROTO_TCP:
		spidx->ul_proto = nxt;
		if (!needport)
			break;
		if (off + sizeof(struct tcphdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(th), (caddr_t)&th);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port = th.th_sport;
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port = th.th_dport;
		break;
	case IPPROTO_UDP:
		spidx->ul_proto = nxt;
		if (!needport)
			break;
		if (off + sizeof(struct udphdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(uh), (caddr_t)&uh);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port = uh.uh_sport;
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port = uh.uh_dport;
		break;
	case IPPROTO_ICMPV6:
		spidx->ul_proto = nxt;
		if (off + sizeof(struct icmp6_hdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(ih), (caddr_t)&ih);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port =
			htons((u_int16_t)ih.icmp6_type);
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port =
			htons((u_int16_t)ih.icmp6_code);
		break;
	default:
		/* XXX intermediate headers??? */
		spidx->ul_proto = nxt;
		break;
	}
}

/* assumes that m is sane */
static int
ipsec6_setspidx_ipaddr(m, spidx)
	struct mbuf *m;
	struct secpolicyindex *spidx;
{
	struct ip6_hdr *ip6 = NULL;
	struct ip6_hdr ip6buf;
	struct sockaddr_in6 *sin6;

	if (m->m_len >= sizeof(*ip6))
		ip6 = mtod(m, struct ip6_hdr *);
	else {
		m_copydata(m, 0, sizeof(ip6buf), (caddr_t)&ip6buf);
		ip6 = &ip6buf;
	}

	sin6 = (struct sockaddr_in6 *)&spidx->src;
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_addr = ip6->ip6_src;
	spidx->prefs = sizeof(struct in6_addr) << 3;

	sin6 = (struct sockaddr_in6 *)&spidx->dst;
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_addr = ip6->ip6_dst;
	spidx->prefd = sizeof(struct in6_addr) << 3;

	return 0;
}
#endif

static struct inpcbpolicy *
ipsec_newpcbpolicy()
{
	struct inpcbpolicy *p;

	p = (struct inpcbpolicy *)malloc(sizeof(*p), M_SECA, M_NOWAIT);
	return p;
}

static void
ipsec_delpcbpolicy(p)
	struct inpcbpolicy *p;
{

	free(p, M_SECA);
}

/* initialize policy in PCB */
int
ipsec_init_pcbpolicy(so, pcb_sp)
	struct socket *so;
	struct inpcbpolicy **pcb_sp;
{
	struct inpcbpolicy *new;
	static int initialized = 0;
	static struct secpolicy *in = NULL, *out = NULL;

	/* sanity check. */
	if (so == NULL || pcb_sp == NULL)
		panic("ipsec_init_pcbpolicy: NULL pointer was passed.");

	if (!initialized) {
		if ((in = key_newsp(0)) == NULL)
			return ENOBUFS;
		if ((out = key_newsp(0)) == NULL) {
			key_freesp(in);
			in = NULL;
			return ENOBUFS;
		}

		in->state = IPSEC_SPSTATE_ALIVE;
		in->policy = IPSEC_POLICY_ENTRUST;
		in->dir = IPSEC_DIR_INBOUND;
		in->readonly = 1;
		in->persist = 1;
		in->so = NULL;

		out->state = IPSEC_SPSTATE_ALIVE;
		out->policy = IPSEC_POLICY_ENTRUST;
		out->dir = IPSEC_DIR_OUTBOUND;
		out->readonly = 1;
		out->persist = 1;
		out->so = NULL;

		initialized++;
	}

	new = ipsec_newpcbpolicy();
	if (new == NULL) {
		ipseclog((LOG_DEBUG, "ipsec_init_pcbpolicy: No more memory.\n"));
		return ENOBUFS;
	}
	bzero(new, sizeof(*new));

	if (so->so_cred != 0 && so->so_cred->cr_uid == 0)
		new->priv = 1;
	else
		new->priv = 0;

	new->sp_in = in;
	new->sp_in->refcnt++;
	new->sp_out = out;
	new->sp_out->refcnt++;

	*pcb_sp = new;

	return 0;
}

/* copy old ipsec policy into new */
int
ipsec_copy_pcbpolicy(old, new)
	struct inpcbpolicy *old, *new;
{

	if (new->sp_in)
		key_freesp(new->sp_in);
	if (old->sp_in->policy == IPSEC_POLICY_IPSEC)
		new->sp_in = ipsec_deepcopy_policy(old->sp_in);
	else {
		new->sp_in = old->sp_in;
		new->sp_in->refcnt++;
	}

	if (new->sp_out)
		key_freesp(new->sp_out);
	if (old->sp_out->policy == IPSEC_POLICY_IPSEC)
		new->sp_out = ipsec_deepcopy_policy(old->sp_out);
	else {
		new->sp_out = old->sp_out;
		new->sp_out->refcnt++;
	}

	new->priv = old->priv;

	return 0;
}

#if 0
static int
ipsec_deepcopy_pcbpolicy(pcb_sp)
	struct inpcbpolicy *pcb_sp;
{
	struct secpolicy *sp;

	sp = ipsec_deepcopy_policy(pcb_sp->sp_in);
	if (sp) {
		key_freesp(pcb_sp->sp_in);
		pcb_sp->sp_in = sp;
	} else
		return ENOBUFS;

	sp = ipsec_deepcopy_policy(pcb_sp->sp_out);
	if (sp) {
		key_freesp(pcb_sp->sp_out);
		pcb_sp->sp_out = sp;
	} else
		return ENOBUFS;

	return 0;
}
#endif

/* deep-copy a policy in PCB */
static struct secpolicy *
ipsec_deepcopy_policy(src)
	struct secpolicy *src;
{
	struct ipsecrequest *newchain = NULL;
	struct ipsecrequest *p;
	struct ipsecrequest **q;
	struct ipsecrequest *r;
	struct secpolicy *dst;

	if (src == NULL)
		return NULL;

	dst = key_newsp(0);
	if (dst == NULL)
		return NULL;

	/*
	 * deep-copy IPsec request chain.  This is required since struct
	 * ipsecrequest is not reference counted.
	 */
	q = &newchain;
	for (p = src->req; p; p = p->next) {
		*q = (struct ipsecrequest *)malloc(sizeof(struct ipsecrequest),
			M_SECA, M_NOWAIT);
		if (*q == NULL)
			goto fail;
		bzero(*q, sizeof(**q));
		(*q)->next = NULL;

		(*q)->saidx.proto = p->saidx.proto;
		(*q)->saidx.mode = p->saidx.mode;
		(*q)->level = p->level;
		(*q)->saidx.reqid = p->saidx.reqid;

		bcopy(&p->saidx.src, &(*q)->saidx.src, sizeof((*q)->saidx.src));
		bcopy(&p->saidx.dst, &(*q)->saidx.dst, sizeof((*q)->saidx.dst));

		(*q)->sav = NULL;
		(*q)->sp = dst;

		q = &((*q)->next);
	}

	if (src->spidx)
		if (keydb_setsecpolicyindex(dst, src->spidx) != 0)
			goto fail;

	dst->req = newchain;
	dst->state = src->state;
	dst->policy = src->policy;
	dst->dir = src->dir;
	dst->so = src->so;
	/* do not touch the refcnt fields */

	return dst;

fail:
	for (p = newchain; p; p = r) {
		r = p->next;
		free(p, M_SECA);
		p = NULL;
	}
	key_freesp(dst);
	return NULL;
}

/* set policy and ipsec request if present. */
static int
ipsec_set_policy(spp, optname, request, len, priv)
	struct secpolicy **spp;
	int optname;
	caddr_t request;
	size_t len;
	int priv;
{
	struct sadb_x_policy *xpl;
	struct secpolicy *newsp = NULL;
	int error;

	/* sanity check. */
	if (spp == NULL || *spp == NULL || request == NULL)
		return EINVAL;
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_set_policy: passed policy\n");
		kdebug_sadb_x_policy((struct sadb_ext *)xpl));

	/* check policy type */
	/* ipsec_set_policy() accepts IPSEC, ENTRUST and BYPASS. */
	if (xpl->sadb_x_policy_type == IPSEC_POLICY_DISCARD ||
	    xpl->sadb_x_policy_type == IPSEC_POLICY_NONE)
		return EINVAL;

	/* check privileged socket */
	if (priv == 0 && xpl->sadb_x_policy_type == IPSEC_POLICY_BYPASS)
		return EACCES;

	/* allocation new SP entry */
	if ((newsp = key_msg2sp(xpl, len, &error)) == NULL)
		return error;

	newsp->state = IPSEC_SPSTATE_ALIVE;

	/* clear old SP and set new SP */
	key_freesp(*spp);
	*spp = newsp;
	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_set_policy: new policy\n");
		kdebug_secpolicy(newsp));

	return 0;
}

static int
ipsec_get_policy(sp, mp)
	struct secpolicy *sp;
	struct mbuf **mp;
{

	/* sanity check. */
	if (sp == NULL || mp == NULL)
		return EINVAL;

	*mp = key_sp2msg(sp);
	if (!*mp) {
		ipseclog((LOG_DEBUG, "ipsec_get_policy: No more memory.\n"));
		return ENOBUFS;
	}

	(*mp)->m_type = MT_DATA;
	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_get_policy:\n");
		kdebug_mbuf(*mp));

	return 0;
}

int
ipsec4_set_policy(inp, optname, request, len, priv)
	struct inpcb *inp;
	int optname;
	caddr_t request;
	size_t len;
	int priv;
{
	struct sadb_x_policy *xpl;
	struct secpolicy **spp;

	/* sanity check. */
	if (inp == NULL || request == NULL)
		return EINVAL;
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		spp = &inp->inp_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		spp = &inp->inp_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec4_set_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	ipsec_invalpcbcache(inp->inp_sp, IPSEC_DIR_ANY);
	return ipsec_set_policy(spp, optname, request, len, priv);
}

int
ipsec4_get_policy(inp, request, len, mp)
	struct inpcb *inp;
	caddr_t request;
	size_t len;
	struct mbuf **mp;
{
	struct sadb_x_policy *xpl;
	struct secpolicy *sp;

	/* sanity check. */
	if (inp == NULL || request == NULL || mp == NULL)
		return EINVAL;
	if (inp->inp_sp == NULL)
		panic("policy in PCB is NULL");
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		sp = inp->inp_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		sp = inp->inp_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec4_get_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	return ipsec_get_policy(sp, mp);
}

/* delete policy in PCB */
int
ipsec4_delete_pcbpolicy(inp)
	struct inpcb *inp;
{
	/* sanity check. */
	if (inp == NULL)
		panic("ipsec4_delete_pcbpolicy: NULL pointer was passed.");

	if (inp->inp_sp == NULL)
		return 0;

	if (inp->inp_sp->sp_in != NULL) {
		key_freesp(inp->inp_sp->sp_in);
		inp->inp_sp->sp_in = NULL;
	}

	if (inp->inp_sp->sp_out != NULL) {
		key_freesp(inp->inp_sp->sp_out);
		inp->inp_sp->sp_out = NULL;
	}

	ipsec_invalpcbcache(inp->inp_sp, IPSEC_DIR_ANY);

	ipsec_delpcbpolicy(inp->inp_sp);
	inp->inp_sp = NULL;

	return 0;
}

#ifdef INET6
int
ipsec6_set_policy(in6p, optname, request, len, priv)
	struct in6pcb *in6p;
	int optname;
	caddr_t request;
	size_t len;
	int priv;
{
	struct sadb_x_policy *xpl;
	struct secpolicy **spp;

	/* sanity check. */
	if (in6p == NULL || request == NULL)
		return EINVAL;
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		spp = &in6p->in6p_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		spp = &in6p->in6p_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec6_set_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	ipsec_invalpcbcache(in6p->in6p_sp, IPSEC_DIR_ANY);
	return ipsec_set_policy(spp, optname, request, len, priv);
}

int
ipsec6_get_policy(in6p, request, len, mp)
	struct in6pcb *in6p;
	caddr_t request;
	size_t len;
	struct mbuf **mp;
{
	struct sadb_x_policy *xpl;
	struct secpolicy *sp;

	/* sanity check. */
	if (in6p == NULL || request == NULL || mp == NULL)
		return EINVAL;
	if (in6p->in6p_sp == NULL)
		panic("policy in PCB is NULL");
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		sp = in6p->in6p_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		sp = in6p->in6p_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec6_get_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	return ipsec_get_policy(sp, mp);
}

int
ipsec6_delete_pcbpolicy(in6p)
	struct in6pcb *in6p;
{
	/* sanity check. */
	if (in6p == NULL)
		panic("ipsec6_delete_pcbpolicy: NULL pointer was passed.");

	if (in6p->in6p_sp == NULL)
		return 0;

	if (in6p->in6p_sp->sp_in != NULL) {
		key_freesp(in6p->in6p_sp->sp_in);
		in6p->in6p_sp->sp_in = NULL;
	}

	if (in6p->in6p_sp->sp_out != NULL) {
		key_freesp(in6p->in6p_sp->sp_out);
		in6p->in6p_sp->sp_out = NULL;
	}

	ipsec_invalpcbcache(in6p->in6p_sp, IPSEC_DIR_ANY);

	ipsec_delpcbpolicy(in6p->in6p_sp);
	in6p->in6p_sp = NULL;

	return 0;
}
#endif

/*
 * return current level.
 * Either IPSEC_LEVEL_USE or IPSEC_LEVEL_REQUIRE are always returned.
 */
u_int
ipsec_get_reqlevel(isr, af)
	struct ipsecrequest *isr;
	int af;
{
	u_int level = 0;
	u_int esp_trans_deflev, esp_net_deflev, ah_trans_deflev, ah_net_deflev;

	/* sanity check */
	if (isr == NULL || isr->sp == NULL)
		panic("ipsec_get_reqlevel: NULL pointer is passed.");

	/* set default level */
	switch (af) {
#ifdef INET
	case AF_INET:
		esp_trans_deflev = ip4_esp_trans_deflev;
		esp_net_deflev = ip4_esp_net_deflev;
		ah_trans_deflev = ip4_ah_trans_deflev;
		ah_net_deflev = ip4_ah_net_deflev;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		esp_trans_deflev = ip6_esp_trans_deflev;
		esp_net_deflev = ip6_esp_net_deflev;
		ah_trans_deflev = ip6_ah_trans_deflev;
		ah_net_deflev = ip6_ah_net_deflev;
		break;
#endif /* INET6 */
	default:
		panic("key_get_reqlevel: Unknown family. %d",
			((struct sockaddr *)&isr->sp->spidx->src)->sa_family);
	}

	/* set level */
	switch (isr->level) {
	case IPSEC_LEVEL_DEFAULT:
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
				level = esp_net_deflev;
			else
				level = esp_trans_deflev;
			break;
		case IPPROTO_AH:
			if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
				level = ah_net_deflev;
			else
				level = ah_trans_deflev;
			break;
		case IPPROTO_IPCOMP:
			/*
			 * we don't really care, as IPcomp document says that
			 * we shouldn't compress small packets
			 */
			level = IPSEC_LEVEL_USE;
			break;
		default:
			panic("ipsec_get_reqlevel: "
				"Illegal protocol defined %u\n",
				isr->saidx.proto);
		}
		break;

	case IPSEC_LEVEL_USE:
	case IPSEC_LEVEL_REQUIRE:
		level = isr->level;
		break;
	case IPSEC_LEVEL_UNIQUE:
		level = IPSEC_LEVEL_REQUIRE;
		break;

	default:
		panic("ipsec_get_reqlevel: Illegal IPsec level %u",
			isr->level);
	}

	return level;
}

/*
 * Check AH/ESP integrity.
 * OUT:
 *	0: valid
 *	1: invalid
 */
static int
ipsec_in_reject(sp, m)
	struct secpolicy *sp;
	struct mbuf *m;
{
	struct ipsecrequest *isr;
	u_int level;
	int need_auth, need_conf, need_icv;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec_in_reject: using SP\n");
		kdebug_secpolicy(sp));

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		return 1;
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		return 0;

	case IPSEC_POLICY_IPSEC:
		break;

	case IPSEC_POLICY_ENTRUST:
	default:
		panic("ipsec_in_reject: Invalid policy found. %d", sp->policy);
	}

	need_auth = 0;
	need_conf = 0;
	need_icv = 0;

	/* XXX should compare policy against ipsec header history */

	for (isr = sp->req; isr != NULL; isr = isr->next) {
		/* get current level */
		level = ipsec_get_reqlevel(isr, AF_INET);

		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			if (level == IPSEC_LEVEL_REQUIRE) {
				need_conf++;

				if (isr->sav != NULL
				 && isr->sav->flags == SADB_X_EXT_NONE
				 && isr->sav->alg_auth != SADB_AALG_NONE)
					need_icv++;
			}
			break;
		case IPPROTO_AH:
			if (level == IPSEC_LEVEL_REQUIRE) {
				need_auth++;
				need_icv++;
			}
			break;
		case IPPROTO_IPCOMP:
			/*
			 * we don't really care, as IPcomp document says that
			 * we shouldn't compress small packets, IPComp policy
			 * should always be treated as being in "use" level.
			 */
			break;
		}
	}

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_in_reject: auth:%d conf:%d icv:%d m_flags:%x\n",
			need_auth, need_conf, need_icv, m->m_flags));

	if ((need_conf && !(m->m_flags & M_DECRYPTED))
	 || (!need_auth && need_icv && !(m->m_flags & M_AUTHIPDGM))
	 || (need_auth && !(m->m_flags & M_AUTHIPHDR)))
		return 1;

	return 0;
}

/*
 * Check AH/ESP integrity.
 * This function is called from tcp_input(), udp_input(),
 * and {ah,esp}4_input for tunnel mode
 */
int
ipsec4_in_reject(m, inp)
	struct mbuf *m;
	struct inpcb *inp;
{
	struct secpolicy *sp = NULL;
	int error;
	int result;

	/* sanity check */
	if (m == NULL)
		return 0;	/* XXX should be panic ? */

	/* get SP for this packet.
	 * When we are called from ip_forward(), we call
	 * ipsec4_getpolicybyaddr() with IP_FORWARDING flag.
	 */
	if (inp == NULL)
		sp = ipsec4_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
		    IP_FORWARDING, &error);
	else
		sp = ipsec4_getpolicybypcb(m, IPSEC_DIR_INBOUND, inp, &error);

	/* XXX should be panic ? -> No, there may be error. */
	if (sp == NULL)
		return 0;

	result = ipsec_in_reject(sp, m);
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP ipsec4_in_reject call free SP:%p\n", sp));
	key_freesp(sp);

	return result;
}

#ifdef INET6
/*
 * Check AH/ESP integrity.
 * This function is called from tcp6_input(), udp6_input(),
 * and {ah,esp}6_input for tunnel mode
 */
int
ipsec6_in_reject(m, in6p)
	struct mbuf *m;
	struct in6pcb *in6p;
{
	struct secpolicy *sp = NULL;
	int error;
	int result;

	/* sanity check */
	if (m == NULL)
		return 0;	/* XXX should be panic ? */

	/* get SP for this packet.
	 * When we are called from ip_forward(), we call
	 * ipsec6_getpolicybyaddr() with IP_FORWARDING flag.
	 */
	if (in6p == NULL)
		sp = ipsec6_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
		    IP_FORWARDING, &error);
	else
		sp = ipsec6_getpolicybypcb(m, IPSEC_DIR_INBOUND, in6p, &error);

	if (sp == NULL)
		return 0;	/* XXX should be panic ? */

	result = ipsec_in_reject(sp, m);
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP ipsec6_in_reject call free SP:%p\n", sp));
	key_freesp(sp);

	return result;
}
#endif

/*
 * compute the byte size to be occupied by IPsec header.
 * in case it is tunneled, it includes the size of outer IP header.
 * NOTE: SP passed is free in this function.
 */
static size_t
ipsec_hdrsiz(sp)
	struct secpolicy *sp;
{
	struct ipsecrequest *isr;
	size_t siz, clen;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec_hdrsiz: using SP\n");
		kdebug_secpolicy(sp));

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		return 0;

	case IPSEC_POLICY_IPSEC:
		break;

	case IPSEC_POLICY_ENTRUST:
	default:
		panic("ipsec_hdrsiz: Invalid policy found. %d", sp->policy);
	}

	siz = 0;

	for (isr = sp->req; isr != NULL; isr = isr->next) {

		clen = 0;

		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
#ifdef IPSEC_ESP
			clen = esp_hdrsiz(isr);
#else
			clen = 0;	/* XXX */
#endif
			break;
		case IPPROTO_AH:
			clen = ah_hdrsiz(isr);
			break;
		case IPPROTO_IPCOMP:
			clen = sizeof(struct ipcomp);
			break;
		}

		if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
			switch (((struct sockaddr *)&isr->saidx.dst)->sa_family) {
			case AF_INET:
				clen += sizeof(struct ip);
				break;
#ifdef INET6
			case AF_INET6:
				clen += sizeof(struct ip6_hdr);
				break;
#endif
			default:
				ipseclog((LOG_ERR, "ipsec_hdrsiz: "
				    "unknown AF %d in IPsec tunnel SA\n",
				    ((struct sockaddr *)&isr->saidx.dst)->sa_family));
				break;
			}
		}
		siz += clen;
	}

	return siz;
}

/* This function is called from ip_forward() and ipsec4_hdrsize_tcp(). */
size_t
ipsec4_hdrsiz(m, dir, inp)
	struct mbuf *m;
	u_int dir;
	struct inpcb *inp;
{
	struct secpolicy *sp = NULL;
	int error;
	size_t size;

	/* sanity check */
	if (m == NULL)
		return 0;	/* XXX should be panic ? */
#if 0
	/* this is possible in TIME_WAIT state */
	if (inp != NULL && inp->inp_socket == NULL)
		panic("ipsec4_hdrsize: why is socket NULL but there is PCB.");
#endif

	/* get SP for this packet.
	 * When we are called from ip_forward(), we call
	 * ipsec4_getpolicybyaddr() with IP_FORWARDING flag.
	 */
	if (inp == NULL)
		sp = ipsec4_getpolicybyaddr(m, dir, IP_FORWARDING, &error);
	else
		sp = ipsec4_getpolicybypcb(m, dir, inp, &error);

	if (sp == NULL)
		return 0;	/* XXX should be panic ? */

	size = ipsec_hdrsiz(sp);
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP ipsec4_hdrsiz call free SP:%p\n", sp));
	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec4_hdrsiz: size:%lu.\n", (unsigned long)size));
	key_freesp(sp);

	return size;
}

#ifdef INET6
/* This function is called from ipsec6_hdrsize_tcp(),
 * and maybe from ip6_forward.()
 */
size_t
ipsec6_hdrsiz(m, dir, in6p)
	struct mbuf *m;
	u_int dir;
	struct in6pcb *in6p;
{
	struct secpolicy *sp = NULL;
	int error;
	size_t size;

	/* sanity check */
	if (m == NULL)
		return 0;	/* XXX should be panic ? */
#if 0
	/* this is possible in TIME_WAIT state */
	if (in6p != NULL && in6p->in6p_socket == NULL)
		panic("ipsec6_hdrsize: why is socket NULL but there is PCB.");
#endif

	/* get SP for this packet */
	/* XXX Is it right to call with IP_FORWARDING. */
	if (in6p == NULL)
		sp = ipsec6_getpolicybyaddr(m, dir, IP_FORWARDING, &error);
	else
		sp = ipsec6_getpolicybypcb(m, dir, in6p, &error);

	if (sp == NULL)
		return 0;
	size = ipsec_hdrsiz(sp);
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP ipsec6_hdrsiz call free SP:%p\n", sp));
	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec6_hdrsiz: size:%lu.\n", (unsigned long)size));
	key_freesp(sp);

	return size;
}
#endif /* INET6 */

#ifdef INET
/*
 * encapsulate for ipsec tunnel.
 * ip->ip_src must be fixed later on.
 */
static int
ipsec4_encapsulate(m, sav)
	struct mbuf *m;
	struct secasvar *sav;
{
	struct ip *oip;
	struct ip *ip;
	size_t hlen;
	size_t plen;

	/* can't tunnel between different AFs */
	if (((struct sockaddr *)&sav->sah->saidx.src)->sa_family
		!= ((struct sockaddr *)&sav->sah->saidx.dst)->sa_family
	 || ((struct sockaddr *)&sav->sah->saidx.src)->sa_family != AF_INET) {
		m_freem(m);
		return EINVAL;
	}
#if 0
	/* XXX if the dst is myself, perform nothing. */
	if (key_ismyaddr((struct sockaddr *)&sav->sah->saidx.dst)) {
		m_freem(m);
		return EINVAL;
	}
#endif

	if (m->m_len < sizeof(*ip))
		panic("ipsec4_encapsulate: assumption failed (first mbuf length)");

	ip = mtod(m, struct ip *);
#ifdef _IP_VHL
	hlen = _IP_VHL_HL(ip->ip_vhl) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif

	if (m->m_len != hlen)
		panic("ipsec4_encapsulate: assumption failed (first mbuf length)");

	/* generate header checksum */
	ip->ip_sum = 0;
#ifdef _IP_VHL
	if (ip->ip_vhl == IP_VHL_BORING)
		ip->ip_sum = in_cksum_hdr(ip);
	else
		ip->ip_sum = in_cksum(m, hlen);
#else
	ip->ip_sum = in_cksum(m, hlen);
#endif

	plen = m->m_pkthdr.len;

	/*
	 * grow the mbuf to accomodate the new IPv4 header.
	 * NOTE: IPv4 options will never be copied.
	 */
	if (M_LEADINGSPACE(m->m_next) < hlen) {
		struct mbuf *n;
		MGET(n, M_DONTWAIT, MT_DATA);
		if (!n) {
			m_freem(m);
			return ENOBUFS;
		}
		n->m_len = hlen;
		n->m_next = m->m_next;
		m->m_next = n;
		m->m_pkthdr.len += hlen;
		oip = mtod(n, struct ip *);
	} else {
		m->m_next->m_len += hlen;
		m->m_next->m_data -= hlen;
		m->m_pkthdr.len += hlen;
		oip = mtod(m->m_next, struct ip *);
	}
	ip = mtod(m, struct ip *);
	ovbcopy((caddr_t)ip, (caddr_t)oip, hlen);
	m->m_len = sizeof(struct ip);
	m->m_pkthdr.len -= (hlen - sizeof(struct ip));

	/* construct new IPv4 header. see RFC 2401 5.1.2.1 */
	/* ECN consideration. */
	ip_ecn_ingress(ip4_ipsec_ecn, &ip->ip_tos, &oip->ip_tos);
#ifdef _IP_VHL
	ip->ip_vhl = IP_MAKE_VHL(IPVERSION, sizeof(struct ip) >> 2);
#else
	ip->ip_hl = sizeof(struct ip) >> 2;
#endif
	ip->ip_off &= htons(~IP_OFFMASK);
	ip->ip_off &= htons(~IP_MF);
	switch (ip4_ipsec_dfbit) {
	case 0:	/* clear DF bit */
		ip->ip_off &= htons(~IP_DF);
		break;
	case 1:	/* set DF bit */
		ip->ip_off |= htons(IP_DF);
		break;
	default:	/* copy DF bit */
		break;
	}
	ip->ip_p = IPPROTO_IPIP;
	if (plen + sizeof(struct ip) < IP_MAXPACKET)
		ip->ip_len = htons(plen + sizeof(struct ip));
	else {
		ipseclog((LOG_ERR, "IPv4 ipsec: size exceeds limit: "
		    "leave ip_len as is (invalid packet)\n"));
	}
	ip->ip_id = ip_newid();
	bcopy(&((struct sockaddr_in *)&sav->sah->saidx.src)->sin_addr,
		&ip->ip_src, sizeof(ip->ip_src));
	bcopy(&((struct sockaddr_in *)&sav->sah->saidx.dst)->sin_addr,
		&ip->ip_dst, sizeof(ip->ip_dst));
	ip->ip_ttl = IPDEFTTL;

	/* XXX Should ip_src be updated later ? */

	return 0;
}
#endif /* INET */

#ifdef INET6
static int
ipsec6_encapsulate(m, sav)
	struct mbuf *m;
	struct secasvar *sav;
{
	struct sockaddr_in6 sa6;
	struct ip6_hdr *oip6;
	struct ip6_hdr *ip6;
	size_t plen;
	int error;

	/* can't tunnel between different AFs */
	if (((struct sockaddr *)&sav->sah->saidx.src)->sa_family
		!= ((struct sockaddr *)&sav->sah->saidx.dst)->sa_family
	 || ((struct sockaddr *)&sav->sah->saidx.src)->sa_family != AF_INET6) {
		m_freem(m);
		return EINVAL;
	}
#if 0
	/* XXX if the dst is myself, perform nothing. */
	if (key_ismyaddr((struct sockaddr *)&sav->sah->saidx.dst)) {
		m_freem(m);
		return EINVAL;
	}
#endif

	plen = m->m_pkthdr.len;

	/*
	 * grow the mbuf to accomodate the new IPv6 header.
	 */
	if (m->m_len != sizeof(struct ip6_hdr))
		panic("ipsec6_encapsulate: assumption failed (first mbuf length)");
	if (M_LEADINGSPACE(m->m_next) < sizeof(struct ip6_hdr)) {
		struct mbuf *n;
		MGET(n, M_DONTWAIT, MT_DATA);
		if (!n) {
			m_freem(m);
			return ENOBUFS;
		}
		n->m_len = sizeof(struct ip6_hdr);
		n->m_next = m->m_next;
		m->m_next = n;
		m->m_pkthdr.len += sizeof(struct ip6_hdr);
		oip6 = mtod(n, struct ip6_hdr *);
	} else {
		m->m_next->m_len += sizeof(struct ip6_hdr);
		m->m_next->m_data -= sizeof(struct ip6_hdr);
		m->m_pkthdr.len += sizeof(struct ip6_hdr);
		oip6 = mtod(m->m_next, struct ip6_hdr *);
	}
	ip6 = mtod(m, struct ip6_hdr *);
	ovbcopy((caddr_t)ip6, (caddr_t)oip6, sizeof(struct ip6_hdr));

	/* XXX: Fake scoped addresses */
	in6_clearscope(&oip6->ip6_src);
	in6_clearscope(&oip6->ip6_dst);

	/* construct new IPv6 header. see RFC 2401 5.1.2.2 */
	/* ECN consideration. */
	ip6_ecn_ingress(ip6_ipsec_ecn, &ip6->ip6_flow, &oip6->ip6_flow);
	if (plen < IPV6_MAXPACKET - sizeof(struct ip6_hdr))
		ip6->ip6_plen = htons(plen);
	else {
		/* ip6->ip6_plen will be updated in ip6_output() */
	}
	ip6->ip6_nxt = IPPROTO_IPV6;

	sa6 = *(struct sockaddr_in6 *)&sav->sah->saidx.src;
	if ((error = sa6_embedscope(&sa6, 0)) != 0)
		return (error);
	ip6->ip6_src = sa6.sin6_addr;

	sa6 = *(struct sockaddr_in6 *)&sav->sah->saidx.dst;
	if ((error = sa6_embedscope(&sa6, 0)) != 0)
		return (error);
	ip6->ip6_dst = sa6.sin6_addr;

	ip6->ip6_hlim = IPV6_DEFHLIM;

	/* XXX Should ip6_src be updated later ? */

	return 0;
}
#endif /* INET6 */

/*
 * Check the variable replay window.
 * ipsec_chkreplay() performs replay check before ICV verification.
 * ipsec_updatereplay() updates replay bitmap.  This must be called after
 * ICV verification (it also performs replay check, which is usually done
 * beforehand).
 * 0 (zero) is returned if packet disallowed, 1 if packet permitted.
 *
 * based on RFC 2401.
 *
 * XXX need to update for 64bit sequence number - 2401bis
 */
int
ipsec_chkreplay(seq, sav)
	u_int32_t seq;
	struct secasvar *sav;
{
	const struct secreplay *replay;
	u_int32_t diff;
	int fr;
	u_int32_t wsizeb;	/* constant: bits of window size */
	int frlast;		/* constant: last frame */

	/* sanity check */
	if (sav == NULL)
		panic("ipsec_chkreplay: NULL pointer was passed.");

	replay = sav->replay;

	if (replay->wsize == 0)
		return 1;	/* no need to check replay. */

	/* constant */
	frlast = replay->wsize - 1;
	wsizeb = replay->wsize << 3;

	/* sequence number of 0 is invalid */
	if (seq == 0)
		return 0;

	/* first time is always okay */
	if (replay->count == 0)
		return 1;

	if (seq > replay->lastseq) {
		/* larger sequences are okay */
		return 1;
	} else {
		/* seq is equal or less than lastseq. */
		diff = replay->lastseq - seq;

		/* over range to check, i.e. too old or wrapped */
		if (diff >= wsizeb)
			return 0;

		fr = frlast - diff / 8;

		/* this packet already seen ? */
		if (replay->bitmap[fr] & (1 << (diff % 8)))
			return 0;

		/* out of order but good */
		return 1;
	}
}

/*
 * check replay counter whether to update or not.
 * OUT:	0:	OK
 *	1:	NG
 * XXX need to update for 64bit sequence number - 2401bis
 */
int
ipsec_updatereplay(seq, sav)
	u_int32_t seq;
	struct secasvar *sav;
{
	struct secreplay *replay;
	u_int64_t diff;
	int fr;
	u_int32_t wsizeb;	/* constant: bits of window size */
	int frlast;		/* constant: last frame */

	/* sanity check */
	if (sav == NULL)
		panic("ipsec_chkreplay: NULL pointer was passed.");

	replay = sav->replay;

	if (replay->wsize == 0)
		goto ok;	/* no need to check replay. */

	/* constant */
	frlast = replay->wsize - 1;
	wsizeb = replay->wsize << 3;

	/* sequence number of 0 is invalid */
	if (seq == 0)
		return 1;

	/* first time */
	if (replay->count == 0) {
		replay->lastseq = seq;
		bzero(replay->bitmap, replay->wsize);
		replay->bitmap[frlast] = 1;
		goto ok;
	}

	if (seq > replay->lastseq) {
		/* seq is larger than lastseq. */
		diff = seq - replay->lastseq;

		/* new larger sequence number */
		if (diff < wsizeb) {
			/* In window */
			/* set bit for this packet */
			vshiftl(replay->bitmap, diff, replay->wsize);
			replay->bitmap[frlast] |= 1;
		} else {
			/* this packet has a "way larger" */
			bzero(replay->bitmap, replay->wsize);
			replay->bitmap[frlast] = 1;
		}
		replay->lastseq = seq;

		/* larger is good */
	} else {
		/* seq is equal or less than lastseq. */
		diff = replay->lastseq - seq;

		/* over range to check, i.e. too old or wrapped */
		if (diff >= wsizeb)
			return 1;

		fr = frlast - diff / 8;

		/* this packet already seen ? */
		if (replay->bitmap[fr] & (1 << (diff % 8)))
			return 1;

		/* mark as seen */
		replay->bitmap[fr] |= (1 << (diff % 8));

		/* out of order but good */
	}

ok:
	if (replay->count == 0xffffffff) {

		/* set overflow flag */
		replay->overflow++;

		/* don't increment, no more packets accepted */
		if ((sav->flags & SADB_X_EXT_CYCSEQ) == 0)
			return 1;

		ipseclog((LOG_WARNING, "replay counter made %d cycle. %s\n",
		    replay->overflow, ipsec_logsastr(sav)));
	}

	replay->count++;

	return 0;
}

/*
 * shift variable length buffer to left.
 * IN:	bitmap: pointer to the buffer
 * 	nbit:	the number of to shift.
 *	wsize:	buffer size (bytes).
 */
static void
vshiftl(bitmap, nbit, wsize)
	unsigned char *bitmap;
	int nbit, wsize;
{
	int s, j, i;
	unsigned char over;

	for (j = 0; j < nbit; j += 8) {
		s = (nbit - j < 8) ? (nbit - j): 8;
		bitmap[0] <<= s;
		for (i = 1; i < wsize; i++) {
			over = (bitmap[i] >> (8 - s));
			bitmap[i] <<= s;
			bitmap[i - 1] |= over;
		}
	}

	return;
}

const char *
ipsec4_logpacketstr(ip, spi)
	struct ip *ip;
	u_int32_t spi;
{
	static char buf[256];
	char *p;
	u_int8_t *s, *d;

	s = (u_int8_t *)(&ip->ip_src);
	d = (u_int8_t *)(&ip->ip_dst);

	p = buf;
	snprintf(buf, sizeof(buf), "packet(SPI=%u ", (u_int32_t)ntohl(spi));
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), "src=%u.%u.%u.%u",
		s[0], s[1], s[2], s[3]);
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), " dst=%u.%u.%u.%u",
		d[0], d[1], d[2], d[3]);
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), ")");

	return buf;
}

#ifdef INET6
const char *
ipsec6_logpacketstr(ip6, spi)
	struct ip6_hdr *ip6;
	u_int32_t spi;
{
	static char buf[256];
	char *p;

	p = buf;
	snprintf(buf, sizeof(buf), "packet(SPI=%u ", (u_int32_t)ntohl(spi));
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), "src=%s",
		ip6_sprintf(&ip6->ip6_src));
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), " dst=%s",
		ip6_sprintf(&ip6->ip6_dst));
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), ")");

	return buf;
}
#endif /* INET6 */

const char *
ipsec_logsastr(sav)
	struct secasvar *sav;
{
	static char buf[256];
	char *p;
	struct secasindex *saidx = &sav->sah->saidx;

	/* validity check */
	if (((struct sockaddr *)&sav->sah->saidx.src)->sa_family
			!= ((struct sockaddr *)&sav->sah->saidx.dst)->sa_family)
		panic("ipsec_logsastr: family mismatched.");

	p = buf;
	snprintf(buf, sizeof(buf), "SA(SPI=%u ", (u_int32_t)ntohl(sav->spi));
	while (*p)
		p++;
	if (((struct sockaddr *)&saidx->src)->sa_family == AF_INET) {
		u_int8_t *s, *d;
		s = (u_int8_t *)&((struct sockaddr_in *)&saidx->src)->sin_addr;
		d = (u_int8_t *)&((struct sockaddr_in *)&saidx->dst)->sin_addr;
		snprintf(p, sizeof(buf) - (p - buf),
			"src=%d.%d.%d.%d dst=%d.%d.%d.%d",
			s[0], s[1], s[2], s[3], d[0], d[1], d[2], d[3]);
	}
#ifdef INET6
	else if (((struct sockaddr *)&saidx->src)->sa_family == AF_INET6) {
		snprintf(p, sizeof(buf) - (p - buf),
			"src=%s",
			ip6_sprintf(&((struct sockaddr_in6 *)&saidx->src)->sin6_addr));
		while (*p)
			p++;
		snprintf(p, sizeof(buf) - (p - buf),
			" dst=%s",
			ip6_sprintf(&((struct sockaddr_in6 *)&saidx->dst)->sin6_addr));
	}
#endif
	while (*p)
		p++;
	snprintf(p, sizeof(buf) - (p - buf), ")");

	return buf;
}

void
ipsec_dumpmbuf(m)
	struct mbuf *m;
{
	int totlen;
	int i;
	u_char *p;

	totlen = 0;
	printf("---\n");
	while (m) {
		p = mtod(m, u_char *);
		for (i = 0; i < m->m_len; i++) {
			printf("%02x ", p[i]);
			totlen++;
			if (totlen % 16 == 0)
				printf("\n");
		}
		m = m->m_next;
	}
	if (totlen % 16 != 0)
		printf("\n");
	printf("---\n");
}

#ifdef INET
static int
ipsec4_checksa(isr, state)
	struct ipsecrequest *isr;
	struct ipsec_output_state *state;
{
	struct ip *ip;
	struct secasindex saidx;
	struct sockaddr_in *sin;

	/* make SA index for search proper SA */
	ip = mtod(state->m, struct ip *);
	bcopy(&isr->saidx, &saidx, sizeof(saidx));
	saidx.mode = isr->saidx.mode;
	saidx.reqid = isr->saidx.reqid;
	sin = (struct sockaddr_in *)&saidx.src;
	if (sin->sin_len == 0) {
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = IPSEC_PORT_ANY;
		bcopy(&ip->ip_src, &sin->sin_addr, sizeof(sin->sin_addr));
	}
	sin = (struct sockaddr_in *)&saidx.dst;
	if (sin->sin_len == 0) {
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = IPSEC_PORT_ANY;
		bcopy(&ip->ip_dst, &sin->sin_addr, sizeof(sin->sin_addr));
	}

	return key_checkrequest(isr, &saidx);
}
/*
 * IPsec output logic for IPv4.
 */
int
ipsec4_output(state, sp, flags)
	struct ipsec_output_state *state;
	struct secpolicy *sp;
	int flags;
{
	struct ip *ip = NULL;
	struct ipsecrequest *isr = NULL;
	int s;
	int error;
	struct sockaddr_in *dst4;

	if (!state)
		panic("state == NULL in ipsec4_output");
	if (!state->m)
		panic("state->m == NULL in ipsec4_output");
	if (!state->ro)
		panic("state->ro == NULL in ipsec4_output");
	if (!state->dst)
		panic("state->dst == NULL in ipsec4_output");
	state->encap = 0;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec4_output: applyed SP\n");
		kdebug_secpolicy(sp));

	for (isr = sp->req; isr != NULL; isr = isr->next) {

#if 0	/* give up to check restriction of transport mode */
	/* XXX but should be checked somewhere */
		/*
		 * some of the IPsec operation must be performed only in
		 * originating case.
		 */
		if (isr->saidx.mode == IPSEC_MODE_TRANSPORT
		 && (flags & IP_FORWARDING))
			continue;
#endif
		error = ipsec4_checksa(isr, state);
		if (error != 0) {
			/*
			 * IPsec processing is required, but no SA found.
			 * I assume that key_acquire() had been called
			 * to get/establish the SA. Here I discard
			 * this packet because it is responsibility for
			 * upper layer to retransmit the packet.
			 */
			ipsecstat.out_nosa++;
			goto bad;
		}

		/* validity check */
		if (isr->sav == NULL) {
			switch (ipsec_get_reqlevel(isr, AF_INET)) {
			case IPSEC_LEVEL_USE:
				continue;
			case IPSEC_LEVEL_REQUIRE:
				/* must be not reached here. */
				panic("ipsec4_output: no SA found, but required.");
			}
		}

		/*
		 * If there is no valid SA, we give up to process any
		 * more.  In such a case, the SA's status is changed
		 * from DYING to DEAD after allocating.  If a packet
		 * send to the receiver by dead SA, the receiver can
		 * not decode a packet because SA has been dead.
		 */
		if (isr->sav->state != SADB_SASTATE_MATURE
		 && isr->sav->state != SADB_SASTATE_DYING) {
			ipsecstat.out_nosa++;
			error = EINVAL;
			goto bad;
		}

		/*
		 * There may be the case that SA status will be changed when
		 * we are refering to one. So calling splsoftnet().
		 */
		s = splnet();

		if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
			/*
			 * build IPsec tunnel.
			 */
			/* XXX should be processed with other familiy */
			if (((struct sockaddr *)&isr->sav->sah->saidx.src)->sa_family != AF_INET) {
				ipseclog((LOG_ERR, "ipsec4_output: "
				    "family mismatched between inner and outer spi=%u\n",
				    (u_int32_t)ntohl(isr->sav->spi)));
				splx(s);
				error = EAFNOSUPPORT;
				goto bad;
			}

			state->m = ipsec4_splithdr(state->m);
			if (!state->m) {
				splx(s);
				error = ENOMEM;
				goto bad;
			}
			error = ipsec4_encapsulate(state->m, isr->sav);
			splx(s);
			if (error) {
				state->m = NULL;
				goto bad;
			}
			ip = mtod(state->m, struct ip *);

			state->ro = &isr->sav->sah->sa_route;
			state->dst = (struct sockaddr *)&state->ro->ro_dst;
			dst4 = (struct sockaddr_in *)state->dst;
			if (state->ro->ro_rt
			 && ((state->ro->ro_rt->rt_flags & RTF_UP) == 0
			  || dst4->sin_addr.s_addr != ip->ip_dst.s_addr)) {
				RTFREE(state->ro->ro_rt);
				state->ro->ro_rt = NULL;
			}
			if (state->ro->ro_rt == 0) {
				dst4->sin_family = AF_INET;
				dst4->sin_len = sizeof(*dst4);
				dst4->sin_addr = ip->ip_dst;
				rtalloc(state->ro);
			}
			if (state->ro->ro_rt == 0) {
				ipstat.ips_noroute++;
				error = EHOSTUNREACH;
				goto bad;
			}

			/* adjust state->dst if tunnel endpoint is offlink */
			if (state->ro->ro_rt->rt_flags & RTF_GATEWAY) {
				state->dst = (struct sockaddr *)state->ro->ro_rt->rt_gateway;
				dst4 = (struct sockaddr_in *)state->dst;
			}

			state->encap++;
		} else
			splx(s);

		state->m = ipsec4_splithdr(state->m);
		if (!state->m) {
			error = ENOMEM;
			goto bad;
		}
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
#ifdef IPSEC_ESP
			if ((error = esp4_output(state->m, isr)) != 0) {
				state->m = NULL;
				goto bad;
			}
			break;
#else
			m_freem(state->m);
			state->m = NULL;
			error = EINVAL;
			goto bad;
#endif
		case IPPROTO_AH:
			if ((error = ah4_output(state->m, isr)) != 0) {
				state->m = NULL;
				goto bad;
			}
			break;
		case IPPROTO_IPCOMP:
			if ((error = ipcomp4_output(state->m, isr)) != 0) {
				state->m = NULL;
				goto bad;
			}
			break;
		default:
			ipseclog((LOG_ERR,
			    "ipsec4_output: unknown ipsec protocol %d\n",
			    isr->saidx.proto));
			m_freem(state->m);
			state->m = NULL;
			error = EINVAL;
			goto bad;
		}

		if (state->m == 0) {
			error = ENOMEM;
			goto bad;
		}
		ip = mtod(state->m, struct ip *);
	}

	return 0;

bad:
	m_freem(state->m);
	state->m = NULL;
	return error;
}
#endif

#ifdef INET6
static int
ipsec6_checksa(isr, state, tunnel)
	struct ipsecrequest *isr;
	struct ipsec_output_state *state;
	int tunnel;
{
	struct ip6_hdr *ip6;
	struct secasindex saidx;
	struct sockaddr_in6 *sin6;

	if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
#ifdef DIAGNOSTIC
		if (!tunnel)
			panic("ipsec6_checksa/inconsistent tunnel attribute");
#endif
		/* When tunnel mode, SA peers must be specified. */
		return key_checkrequest(isr, &isr->saidx);
	}

	/* make SA index for search proper SA */
	ip6 = mtod(state->m, struct ip6_hdr *);
	if (tunnel) {
		bzero(&saidx, sizeof(saidx));
		saidx.proto = isr->saidx.proto;
	} else
		bcopy(&isr->saidx, &saidx, sizeof(saidx));
	saidx.mode = isr->saidx.mode;
	saidx.reqid = isr->saidx.reqid;
	sin6 = (struct sockaddr_in6 *)&saidx.src;
	if (sin6->sin6_len == 0 || tunnel) {
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = IPSEC_PORT_ANY;
		sin6->sin6_addr = ip6->ip6_src;
	}
	sin6 = (struct sockaddr_in6 *)&saidx.dst;
	if (sin6->sin6_len == 0 || tunnel) {
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = IPSEC_PORT_ANY;
		sin6->sin6_addr = ip6->ip6_dst;
	}

	return key_checkrequest(isr, &saidx);
}

/*
 * IPsec output logic for IPv6, transport mode.
 */
int
ipsec6_output_trans(state, nexthdrp, mprev, sp, flags, tun)
	struct ipsec_output_state *state;
	u_char *nexthdrp;
	struct mbuf *mprev;
	struct secpolicy *sp;
	int flags;
	int *tun;
{
	struct ip6_hdr *ip6;
	struct ipsecrequest *isr = NULL;
	int error = 0;
	int plen;

	if (!state)
		panic("state == NULL in ipsec6_output_trans");
	if (!state->m)
		panic("state->m == NULL in ipsec6_output_trans");
	if (!nexthdrp)
		panic("nexthdrp == NULL in ipsec6_output_trans");
	if (!mprev)
		panic("mprev == NULL in ipsec6_output_trans");
	if (!sp)
		panic("sp == NULL in ipsec6_output_trans");
	if (!tun)
		panic("tun == NULL in ipsec6_output_trans");

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec6_output_trans: applyed SP\n");
		kdebug_secpolicy(sp));

	*tun = 0;
	for (isr = sp->req; isr; isr = isr->next) {
		if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
			/* the rest will be handled by ipsec6_output_tunnel() */
			break;
		}

		error = ipsec6_checksa(isr, state, 0);
		if (error == EIO)
			goto bad;
		if (error == ENOENT) {
			/*
			 * IPsec processing is required, but no SA found.
			 * I assume that key_acquire() had been called
			 * to get/establish the SA. Here I discard
			 * this packet because it is responsibility for
			 * upper layer to retransmit the packet.
			 */
			ipsec6stat.out_nosa++;

			/*
			 * Notify the fact that the packet is discarded
			 * to ourselves. I believe this is better than
			 * just silently discarding. (jinmei@kame.net)
			 * XXX: should we restrict the error to TCP packets?
			 * XXX: should we directly notify sockets via
			 *      pfctlinputs?
			 *
			 * Noone have initialized rcvif until this point,
			 * so clear it.
			 */
			if ((state->m->m_flags & M_PKTHDR) != 0)
				state->m->m_pkthdr.rcvif = NULL;
			icmp6_error(state->m, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADMIN, 0);
			state->m = NULL; /* icmp6_error freed the mbuf */
			goto bad;
		}

		/* validity check */
		if (isr->sav == NULL) {
			switch (ipsec_get_reqlevel(isr, AF_INET6)) {
			case IPSEC_LEVEL_USE:
				continue;
			case IPSEC_LEVEL_REQUIRE:
				/* must be not reached here. */
				panic("ipsec6_output_trans: no SA found, but required.");
			}
		}

		/*
		 * If there is no valid SA, we give up to process.
		 * see same place at ipsec4_output().
		 */
		if (isr->sav->state != SADB_SASTATE_MATURE
		 && isr->sav->state != SADB_SASTATE_DYING) {
			ipsec6stat.out_nosa++;
			error = EINVAL;
			goto bad;
		}

		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
#ifdef IPSEC_ESP
			error = esp6_output(state->m, nexthdrp, mprev->m_next, isr);
#else
			m_freem(state->m);
			error = EINVAL;
#endif
			break;
		case IPPROTO_AH:
			error = ah6_output(state->m, nexthdrp, mprev->m_next, isr);
			break;
		case IPPROTO_IPCOMP:
			error = ipcomp6_output(state->m, nexthdrp, mprev->m_next, isr);
			break;
		default:
			ipseclog((LOG_ERR, "ipsec6_output_trans: "
			    "unknown ipsec protocol %d\n", isr->saidx.proto));
			m_freem(state->m);
			ipsec6stat.out_inval++;
			error = EINVAL;
			break;
		}
		if (error) {
			state->m = NULL;
			goto bad;
		}
		plen = state->m->m_pkthdr.len - sizeof(struct ip6_hdr);
		if (plen > IPV6_MAXPACKET) {
			ipseclog((LOG_ERR, "ipsec6_output_trans: "
			    "IPsec with IPv6 jumbogram is not supported\n"));
			ipsec6stat.out_inval++;
			error = EINVAL;	/* XXX */
			goto bad;
		}
		ip6 = mtod(state->m, struct ip6_hdr *);
		ip6->ip6_plen = htons(plen);
	}

	/* if we have more to go, we need a tunnel mode processing */
	if (isr != NULL)
		*tun = 1;

	return 0;

bad:
	m_freem(state->m);
	state->m = NULL;
	return error;
}

/*
 * IPsec output logic for IPv6, tunnel mode.
 */
int
ipsec6_output_tunnel(state, sp, flags)
	struct ipsec_output_state *state;
	struct secpolicy *sp;
	int flags;
{
	struct ip6_hdr *ip6;
	struct ipsecrequest *isr = NULL;
	int error = 0;
	int plen;
	struct sockaddr_in6 *dst6;
	int s;

	if (!state)
		panic("state == NULL in ipsec6_output_tunnel");
	if (!state->m)
		panic("state->m == NULL in ipsec6_output_tunnel");
	if (!sp)
		panic("sp == NULL in ipsec6_output_tunnel");

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec6_output_tunnel: applyed SP\n");
		kdebug_secpolicy(sp));

	/*
	 * transport mode ipsec (before the 1st tunnel mode) is already
	 * processed by ipsec6_output_trans().
	 */
	for (isr = sp->req; isr; isr = isr->next) {
		if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
			break;
	}

	for (/* already initialized */; isr; isr = isr->next) {
		error = ipsec6_checksa(isr, state, 1);
		if (error == EIO)
			goto bad;
		if (error == ENOENT) {
			/*
			 * IPsec processing is required, but no SA found.
			 * I assume that key_acquire() had been called
			 * to get/establish the SA. Here I discard
			 * this packet because it is responsibility for
			 * upper layer to retransmit the packet.
			 */
			ipsec6stat.out_nosa++;
			error = ENOENT;
			goto bad;
		}

		/* validity check */
		if (isr->sav == NULL) {
			switch (ipsec_get_reqlevel(isr, AF_INET6)) {
			case IPSEC_LEVEL_USE:
				continue;
			case IPSEC_LEVEL_REQUIRE:
				/* must be not reached here. */
				panic("ipsec6_output_tunnel: no SA found, but required.");
			}
		}

		/*
		 * If there is no valid SA, we give up to process.
		 * see same place at ipsec4_output().
		 */
		if (isr->sav->state != SADB_SASTATE_MATURE
		 && isr->sav->state != SADB_SASTATE_DYING) {
			ipsec6stat.out_nosa++;
			error = EINVAL;
			goto bad;
		}

		/*
		 * There may be the case that SA status will be changed when
		 * we are refering to one. So calling splsoftnet().
		 */
		s = splnet();

		if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
			/*
			 * build IPsec tunnel.
			 */
			/* XXX should be processed with other familiy */
			if (((struct sockaddr *)&isr->sav->sah->saidx.src)->sa_family != AF_INET6) {
				ipseclog((LOG_ERR, "ipsec6_output_tunnel: "
				    "family mismatched between inner and outer, spi=%u\n",
				    (u_int32_t)ntohl(isr->sav->spi)));
				splx(s);
				ipsec6stat.out_inval++;
				error = EAFNOSUPPORT;
				goto bad;
			}

			state->m = ipsec6_splithdr(state->m);
			if (!state->m) {
				splx(s);
				ipsec6stat.out_nomem++;
				error = ENOMEM;
				goto bad;
			}
			error = ipsec6_encapsulate(state->m, isr->sav);
			splx(s);
			if (error) {
				state->m = 0;
				goto bad;
			}
			ip6 = mtod(state->m, struct ip6_hdr *);

			state->ro = &isr->sav->sah->sa_route;
			state->dst = (struct sockaddr *)&state->ro->ro_dst;
			dst6 = (struct sockaddr_in6 *)state->dst;
			if (state->ro->ro_rt &&
			    (!(state->ro->ro_rt->rt_flags & RTF_UP) ||
			     !IN6_ARE_ADDR_EQUAL(&dst6->sin6_addr,
						 &ip6->ip6_dst))) {
				RTFREE(state->ro->ro_rt);
				state->ro->ro_rt = NULL;
			}
			if (state->ro->ro_rt == 0) {
				bzero(dst6, sizeof(*dst6));
				dst6->sin6_family = AF_INET6;
				dst6->sin6_len = sizeof(*dst6);
				dst6->sin6_addr = ip6->ip6_dst;
				rtalloc(state->ro);
			}
			if (state->ro->ro_rt == 0) {
				ip6stat.ip6s_noroute++;
				ipsec6stat.out_noroute++;
				error = EHOSTUNREACH;
				goto bad;
			}

			/* adjust state->dst if tunnel endpoint is offlink */
			if (state->ro->ro_rt->rt_flags & RTF_GATEWAY) {
				state->dst = (struct sockaddr *)state->ro->ro_rt->rt_gateway;
				dst6 = (struct sockaddr_in6 *)state->dst;
			}
		} else
			splx(s);

		state->m = ipsec6_splithdr(state->m);
		if (!state->m) {
			ipsec6stat.out_nomem++;
			error = ENOMEM;
			goto bad;
		}
		ip6 = mtod(state->m, struct ip6_hdr *);
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
#ifdef IPSEC_ESP
			error = esp6_output(state->m, &ip6->ip6_nxt,
			    state->m->m_next, isr);
#else
			m_freem(state->m);
			error = EINVAL;
#endif
			break;
		case IPPROTO_AH:
			error = ah6_output(state->m, &ip6->ip6_nxt,
			    state->m->m_next, isr);
			break;
		case IPPROTO_IPCOMP:
			/* XXX code should be here */
			/* FALLTHROUGH */
		default:
			ipseclog((LOG_ERR, "ipsec6_output_tunnel: "
			    "unknown ipsec protocol %d\n", isr->saidx.proto));
			m_freem(state->m);
			ipsec6stat.out_inval++;
			error = EINVAL;
			break;
		}
		if (error) {
			state->m = NULL;
			goto bad;
		}
		plen = state->m->m_pkthdr.len - sizeof(struct ip6_hdr);
		if (plen > IPV6_MAXPACKET) {
			ipseclog((LOG_ERR, "ipsec6_output_tunnel: "
			    "IPsec with IPv6 jumbogram is not supported\n"));
			ipsec6stat.out_inval++;
			error = EINVAL;	/* XXX */
			goto bad;
		}
		ip6 = mtod(state->m, struct ip6_hdr *);
		ip6->ip6_plen = htons(plen);
	}

	return 0;

bad:
	m_freem(state->m);
	state->m = NULL;
	return error;
}
#endif /* INET6 */

#ifdef INET
/*
 * Chop IP header and option off from the payload.
 */
static struct mbuf *
ipsec4_splithdr(m)
	struct mbuf *m;
{
	struct mbuf *mh;
	struct ip *ip;
	int hlen;

	if (m->m_len < sizeof(struct ip))
		panic("ipsec4_splithdr: first mbuf too short");
	ip = mtod(m, struct ip *);
#ifdef _IP_VHL
	hlen = _IP_VHL_HL(ip->ip_vhl) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif
	if (m->m_len > hlen) {
		MGETHDR(mh, M_DONTWAIT, MT_HEADER);
		if (!mh) {
			m_freem(m);
			return NULL;
		}
		M_MOVE_PKTHDR(mh, m);
		MH_ALIGN(mh, hlen);
		m->m_len -= hlen;
		m->m_data += hlen;
		mh->m_next = m;
		m = mh;
		m->m_len = hlen;
		bcopy((caddr_t)ip, mtod(m, caddr_t), hlen);
	} else if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (!m)
			return NULL;
	}
	return m;
}
#endif

#ifdef INET6
static struct mbuf *
ipsec6_splithdr(m)
	struct mbuf *m;
{
	struct mbuf *mh;
	struct ip6_hdr *ip6;
	int hlen;

	if (m->m_len < sizeof(struct ip6_hdr))
		panic("ipsec6_splithdr: first mbuf too short");
	ip6 = mtod(m, struct ip6_hdr *);
	hlen = sizeof(struct ip6_hdr);
	if (m->m_len > hlen) {
		MGETHDR(mh, M_DONTWAIT, MT_HEADER);
		if (!mh) {
			m_freem(m);
			return NULL;
		}
		M_MOVE_PKTHDR(mh, m);
		MH_ALIGN(mh, hlen);
		m->m_len -= hlen;
		m->m_data += hlen;
		mh->m_next = m;
		m = mh;
		m->m_len = hlen;
		bcopy((caddr_t)ip6, mtod(m, caddr_t), hlen);
	} else if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (!m)
			return NULL;
	}
	return m;
}
#endif

/* validate inbound IPsec tunnel packet. */
int
ipsec4_tunnel_validate(m, off, nxt0, sav)
	struct mbuf *m;		/* no pullup permitted, m->m_len >= ip */
	int off;
	u_int nxt0;
	struct secasvar *sav;
{
	u_int8_t nxt = nxt0 & 0xff;
	struct sockaddr_in *sin;
	struct sockaddr_in osrc, odst, isrc, idst;
	int hlen;
	struct secpolicy *sp;
	struct ip *oip;

#ifdef DIAGNOSTIC
	if (m->m_len < sizeof(struct ip))
		panic("too short mbuf on ipsec4_tunnel_validate");
#endif
	if (nxt != IPPROTO_IPV4)
		return 0;
	if (m->m_pkthdr.len < off + sizeof(struct ip))
		return 0;
	/* do not decapsulate if the SA is for transport mode only */
	if (sav->sah->saidx.mode == IPSEC_MODE_TRANSPORT)
		return 0;

	oip = mtod(m, struct ip *);
	hlen = oip->ip_hl << 2;
	if (hlen != sizeof(struct ip))
		return 0;

	/* AF_INET6 should be supported, but at this moment we don't. */
	sin = (struct sockaddr_in *)&sav->sah->saidx.dst;
	if (sin->sin_family != AF_INET)
		return 0;
	if (bcmp(&oip->ip_dst, &sin->sin_addr, sizeof(oip->ip_dst)) != 0)
		return 0;

	/* XXX slow */
	bzero(&osrc, sizeof(osrc));
	bzero(&odst, sizeof(odst));
	bzero(&isrc, sizeof(isrc));
	bzero(&idst, sizeof(idst));
	osrc.sin_family = odst.sin_family = isrc.sin_family = idst.sin_family =
	    AF_INET;
	osrc.sin_len = odst.sin_len = isrc.sin_len = idst.sin_len =
	    sizeof(struct sockaddr_in);
	osrc.sin_addr = oip->ip_src;
	odst.sin_addr = oip->ip_dst;
	m_copydata(m, off + offsetof(struct ip, ip_src), sizeof(isrc.sin_addr),
	    (caddr_t)&isrc.sin_addr);
	m_copydata(m, off + offsetof(struct ip, ip_dst), sizeof(idst.sin_addr),
	    (caddr_t)&idst.sin_addr);

	/*
	 * RFC2401 5.2.1 (b): (assume that we are using tunnel mode)
	 * - if the inner destination is multicast address, there can be
	 *   multiple permissible inner source address.  implementation
	 *   may want to skip verification of inner source address against
	 *   SPD selector.
	 * - if the inner protocol is ICMP, the packet may be an error report
	 *   from routers on the other side of the VPN cloud (R in the
	 *   following diagram).  in this case, we cannot verify inner source
	 *   address against SPD selector.
	 *	me -- gw === gw -- R -- you
	 *
	 * we consider the first bullet to be users responsibility on SPD entry
	 * configuration (if you need to encrypt multicast traffic, set
	 * the source range of SPD selector to 0.0.0.0/0, or have explicit
	 * address ranges for possible senders).
	 * the second bullet is not taken care of (yet).
	 *
	 * therefore, we do not do anything special about inner source.
	 */

	sp = key_gettunnel((struct sockaddr *)&osrc, (struct sockaddr *)&odst,
	    (struct sockaddr *)&isrc, (struct sockaddr *)&idst);
	/*
	 * when there is no suitable inbound policy for the packet of the ipsec
	 * tunnel mode, the kernel never decapsulate the tunneled packet
	 * as the ipsec tunnel mode even when the system wide policy is "none".
	 * then the kernel leaves the generic tunnel module to process this
	 * packet.  if there is no rule of the generic tunnel, the packet
	 * is rejected and the statistics will be counted up.
	 */
	if (!sp)
		return 0;
	key_freesp(sp);

	return 1;
}

#ifdef INET6
/* validate inbound IPsec tunnel packet. */
int
ipsec6_tunnel_validate(m, off, nxt0, sav)
	struct mbuf *m;		/* no pullup permitted, m->m_len >= ip */
	int off;
	u_int nxt0;
	struct secasvar *sav;
{
	u_int8_t nxt = nxt0 & 0xff;
	struct sockaddr_in6 *sin6;
	struct sockaddr_in6 osrc, odst, isrc, idst;
	struct secpolicy *sp;
	struct ip6_hdr *oip6;

#ifdef DIAGNOSTIC
	if (m->m_len < sizeof(struct ip6_hdr))
		panic("too short mbuf on ipsec6_tunnel_validate");
#endif
	if (nxt != IPPROTO_IPV6)
		return 0;
	if (m->m_pkthdr.len < off + sizeof(struct ip6_hdr))
		return 0;
	/* do not decapsulate if the SA is for transport mode only */
	if (sav->sah->saidx.mode == IPSEC_MODE_TRANSPORT)
		return 0;

	oip6 = mtod(m, struct ip6_hdr *);

	/* AF_INET should be supported, but at this moment we don't. */
	sin6 = (struct sockaddr_in6 *)&sav->sah->saidx.dst;
	if (sin6->sin6_family != AF_INET6)
		return 0;
	if (!IN6_ARE_ADDR_EQUAL(&oip6->ip6_dst, &sin6->sin6_addr))
		return 0;

	/* XXX slow */
	bzero(&osrc, sizeof(osrc));
	bzero(&odst, sizeof(odst));
	bzero(&isrc, sizeof(isrc));
	bzero(&idst, sizeof(idst));
	osrc.sin6_family = odst.sin6_family = isrc.sin6_family =
	    idst.sin6_family = AF_INET6;
	osrc.sin6_len = odst.sin6_len = isrc.sin6_len = idst.sin6_len = 
	    sizeof(struct sockaddr_in6);
	osrc.sin6_addr = oip6->ip6_src;
	odst.sin6_addr = oip6->ip6_dst;
	m_copydata(m, off + offsetof(struct ip6_hdr, ip6_src),
	    sizeof(isrc.sin6_addr), (caddr_t)&isrc.sin6_addr);
	m_copydata(m, off + offsetof(struct ip6_hdr, ip6_dst),
	    sizeof(idst.sin6_addr), (caddr_t)&idst.sin6_addr);

	/*
	 * regarding to inner source address validation, see a long comment
	 * in ipsec4_tunnel_validate.
	 */

	sp = key_gettunnel((struct sockaddr *)&osrc, (struct sockaddr *)&odst,
	    (struct sockaddr *)&isrc, (struct sockaddr *)&idst);
	if (!sp)
		return 0;
	key_freesp(sp);

	return 1;
}
#endif

/*
 * Make a mbuf chain for encryption.
 * If the original mbuf chain contains a mbuf with a cluster,
 * allocate a new cluster and copy the data to the new cluster.
 * XXX: this hack is inefficient, but is necessary to handle cases
 * of TCP retransmission...
 */
struct mbuf *
ipsec_copypkt(m)
	struct mbuf *m;
{
	struct mbuf *n, **mpp, *mnew;

	for (n = m, mpp = &m; n; n = n->m_next) {
		if (n->m_flags & M_EXT) {
			/*
			 * Make a copy only if there is more than one
			 * references to the cluster.
			 * XXX: is this approach effective?
			 */
			if (!M_WRITABLE(n)) {
				int remain, copied;
				struct mbuf *mm;

				if (n->m_flags & M_PKTHDR) {
					MGETHDR(mnew, M_DONTWAIT, MT_HEADER);
					if (mnew == NULL)
						goto fail;
					M_MOVE_PKTHDR(mnew, n);
				}
				else {
					MGET(mnew, M_DONTWAIT, MT_DATA);
					if (mnew == NULL)
						goto fail;
				}
				mnew->m_len = 0;
				mm = mnew;

				/*
				 * Copy data. If we don't have enough space to
				 * store the whole data, allocate a cluster
				 * or additional mbufs.
				 * XXX: we don't use m_copyback(), since the
				 * function does not use clusters and thus is
				 * inefficient.
				 */
				remain = n->m_len;
				copied = 0;
				while (1) {
					int len;
					struct mbuf *mn;

					if (remain <= (mm->m_flags & M_PKTHDR ? MHLEN : MLEN))
						len = remain;
					else { /* allocate a cluster */
						MCLGET(mm, M_DONTWAIT);
						if (!(mm->m_flags & M_EXT)) {
							m_free(mm);
							goto fail;
						}
						len = remain < MCLBYTES ?
							remain : MCLBYTES;
					}

					bcopy(n->m_data + copied, mm->m_data,
					      len);

					copied += len;
					remain -= len;
					mm->m_len = len;

					if (remain <= 0) /* completed? */
						break;

					/* need another mbuf */
					MGETHDR(mn, M_DONTWAIT, MT_HEADER);
					if (mn == NULL)
						goto fail;
					mn->m_pkthdr.rcvif = NULL;
					mm->m_next = mn;
					mm = mn;
				}

				/* adjust chain */
				mm->m_next = m_free(n);
				n = mm;
				*mpp = mnew;
				mpp = &n->m_next;

				continue;
			}
		}
		*mpp = n;
		mpp = &n->m_next;
	}

	return (m);
  fail:
	m_freem(m);
	return (NULL);
}

static struct ipsecaux *
ipsec_addaux(m)
	struct mbuf *m;
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_IPSEC_HISTORY, NULL);
	if (mtag == NULL) {
		mtag = m_tag_get(PACKET_TAG_IPSEC_HISTORY,
		    sizeof(struct ipsecaux), M_NOWAIT);
		if (mtag != NULL)
			m_tag_prepend(m, mtag);
	}
	if (mtag == NULL)
		return NULL;	/* ENOBUFS */
	/* XXX is this necessary? */
	bzero((void *)(mtag + 1), sizeof(struct ipsecaux));
	return mtag ? (struct ipsecaux *)(mtag + 1) : NULL;
}

static struct ipsecaux *
ipsec_findaux(m)
	struct mbuf *m;
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_IPSEC_HISTORY, NULL);
	return mtag ? (struct ipsecaux *)(mtag + 1) : NULL;
}

void
ipsec_delaux(m)
	struct mbuf *m;
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_IPSEC_HISTORY, NULL);
	if (mtag != NULL)
		m_tag_delete(m, mtag);
}

/* if the aux buffer is unnecessary, nuke it. */
static void
ipsec_optaux(m, aux)
	struct mbuf *m;
	struct ipsecaux *aux;
{

	if (aux == NULL)
		return;
	ipsec_delaux(m);
}

int
ipsec_addhist(m, proto, spi)
	struct mbuf *m;
	int proto;
	u_int32_t spi;
{
	struct ipsecaux *aux;

	aux = ipsec_addaux(m);
	if (aux == NULL)
		return ENOBUFS;
	aux->hdrs++;
	return 0;
}

int
ipsec_getnhist(m)
	struct mbuf *m;
{
	struct ipsecaux *aux;

	aux = ipsec_findaux(m);
	if (aux == NULL)
		return 0;
	return aux->hdrs;
}

void
ipsec_clearhist(m)
	struct mbuf *m;
{
	struct ipsecaux *aux;

	aux = ipsec_findaux(m);
	ipsec_optaux(m, aux);
}
