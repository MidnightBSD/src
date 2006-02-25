/*	$FreeBSD: src/sys/netinet6/esp_output.c,v 1.13 2005/01/07 02:30:34 imp Exp $	*/
/*	$KAME: esp_output.c,v 1.44 2001/07/26 06:53:15 jinmei Exp $	*/

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

/*
 * RFC1827/2406 Encapsulated Security Payload.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
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
#include <netinet6/esp.h>
#ifdef INET6
#include <netinet6/esp6.h>
#endif
#include <netkey/key.h>
#include <netkey/keydb.h>

#include <net/net_osdep.h>

static int esp_output __P((struct mbuf *, u_char *, struct mbuf *,
	struct ipsecrequest *, int));

/*
 * compute ESP header size.
 */
size_t
esp_hdrsiz(isr)
	struct ipsecrequest *isr;
{
	struct secasvar *sav;
	const struct esp_algorithm *algo;
	const struct ah_algorithm *aalgo;
	size_t ivlen;
	size_t authlen;
	size_t hdrsiz;

	/* sanity check */
	if (isr == NULL)
		panic("esp_hdrsiz: NULL was passed.");

	sav = isr->sav;

	if (isr->saidx.proto != IPPROTO_ESP)
		panic("unsupported mode passed to esp_hdrsiz");

	if (sav == NULL)
		goto estimate;
	if (sav->state != SADB_SASTATE_MATURE
	 && sav->state != SADB_SASTATE_DYING)
		goto estimate;

	/* we need transport mode ESP. */
	algo = esp_algorithm_lookup(sav->alg_enc);
	if (!algo)
		goto estimate;
	ivlen = sav->ivlen;
	if (ivlen < 0)
		goto estimate;

	/*
	 * XXX
	 * right now we don't calcurate the padding size.  simply
	 * treat the padding size as constant, for simplicity.
	 *
	 * XXX variable size padding support
	 */
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1827 */
		hdrsiz = sizeof(struct esp) + ivlen + 9;
	} else {
		/* RFC 2406 */
		aalgo = ah_algorithm_lookup(sav->alg_auth);
		if (aalgo && sav->replay && sav->key_auth)
			authlen = (aalgo->sumsiz)(sav);
		else
			authlen = 0;
		hdrsiz = sizeof(struct newesp) + ivlen + 9 + authlen;
	}

	return hdrsiz;

   estimate:
	/*
	 * ASSUMING:
	 *	sizeof(struct newesp) > sizeof(struct esp).
	 *	esp_max_ivlen() = max ivlen for CBC mode
	 *	9 = (maximum padding length without random padding length)
	 *	   + (Pad Length field) + (Next Header field).
	 *	16 = maximum ICV we support.
	 */
	return sizeof(struct newesp) + esp_max_ivlen() + 9 + 16;
}

/*
 * Modify the packet so that the payload is encrypted.
 * The mbuf (m) must start with IPv4 or IPv6 header.
 * On failure, free the given mbuf and return NULL.
 *
 * on invocation:
 *	m   nexthdrp md
 *	v   v        v
 *	IP ......... payload
 * during the encryption:
 *	m   nexthdrp mprev md
 *	v   v        v     v
 *	IP ............... esp iv payload pad padlen nxthdr
 *	                   <--><-><------><--------------->
 *	                   esplen plen    extendsiz
 *	                       ivlen
 *	                   <-----> esphlen
 *	<-> hlen
 *	<-----------------> espoff
 */
static int
esp_output(m, nexthdrp, md, isr, af)
	struct mbuf *m;
	u_char *nexthdrp;
	struct mbuf *md;
	struct ipsecrequest *isr;
	int af;
{
	struct mbuf *n;
	struct mbuf *mprev;
	struct esp *esp;
	struct esptail *esptail;
	struct secasvar *sav = isr->sav;
	const struct esp_algorithm *algo;
	u_int32_t spi;
	u_int8_t nxt = 0;
	size_t plen;	/* payload length to be encrypted */
	size_t espoff;
	int ivlen;
	int afnumber;
	size_t extendsiz;
	int error = 0;
	struct ipsecstat *stat;

	switch (af) {
#ifdef INET
	case AF_INET:
		afnumber = 4;
		stat = &ipsecstat;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		afnumber = 6;
		stat = &ipsec6stat;
		break;
#endif
	default:
		ipseclog((LOG_ERR, "esp_output: unsupported af %d\n", af));
		return 0;	/* no change at all */
	}

	/* some sanity check */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && !sav->replay) {
		switch (af) {
#ifdef INET
		case AF_INET:
		    {
			struct ip *ip;

			ip = mtod(m, struct ip *);
			ipseclog((LOG_DEBUG, "esp4_output: internal error: "
				"sav->replay is null: %x->%x, SPI=%u\n",
				(u_int32_t)ntohl(ip->ip_src.s_addr),
				(u_int32_t)ntohl(ip->ip_dst.s_addr),
				(u_int32_t)ntohl(sav->spi)));
			ipsecstat.out_inval++;
			break;
		    }
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			ipseclog((LOG_DEBUG, "esp6_output: internal error: "
				"sav->replay is null: SPI=%u\n",
				(u_int32_t)ntohl(sav->spi)));
			ipsec6stat.out_inval++;
			break;
#endif /* INET6 */
		default:
			panic("esp_output: should not reach here");
		}
		m_freem(m);
		return EINVAL;
	}

	algo = esp_algorithm_lookup(sav->alg_enc);
	if (!algo) {
		ipseclog((LOG_ERR, "esp_output: unsupported algorithm: "
		    "SPI=%u\n", (u_int32_t)ntohl(sav->spi)));
		m_freem(m);
		return EINVAL;
	}
	spi = sav->spi;
	ivlen = sav->ivlen;
	/* should be okey */
	if (ivlen < 0) {
		panic("invalid ivlen");
	}

    {
	/*
	 * insert ESP header.
	 * XXX inserts ESP header right after IPv4 header.  should
	 * chase the header chain.
	 * XXX sequential number
	 */
#ifdef INET
	struct ip *ip = NULL;
#endif
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
	size_t esplen;	/* sizeof(struct esp/newesp) */
	size_t esphlen;	/* sizeof(struct esp/newesp) + ivlen */
	size_t hlen = 0;	/* ip header len */

	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1827 */
		esplen = sizeof(struct esp);
	} else {
		/* RFC 2406 */
		if (sav->flags & SADB_X_EXT_DERIV)
			esplen = sizeof(struct esp);
		else
			esplen = sizeof(struct newesp);
	}
	esphlen = esplen + ivlen;

	for (mprev = m; mprev && mprev->m_next != md; mprev = mprev->m_next)
		;
	if (mprev == NULL || mprev->m_next != md) {
		ipseclog((LOG_DEBUG, "esp%d_output: md is not in chain\n",
		    afnumber));
		m_freem(m);
		return EINVAL;
	}

	plen = 0;
	for (n = md; n; n = n->m_next)
		plen += n->m_len;

	switch (af) {
#ifdef INET
	case AF_INET:
		ip = mtod(m, struct ip *);
#ifdef _IP_VHL
		hlen = IP_VHL_HL(ip->ip_vhl) << 2;
#else
		hlen = ip->ip_hl << 2;
#endif
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		hlen = sizeof(*ip6);
		break;
#endif
	}

	/* make the packet over-writable */
	mprev->m_next = NULL;
	if ((md = ipsec_copypkt(md)) == NULL) {
		m_freem(m);
		error = ENOBUFS;
		goto fail;
	}
	mprev->m_next = md;

	espoff = m->m_pkthdr.len - plen;

	/*
	 * grow the mbuf to accomodate ESP header.
	 * before: IP ... payload
	 * after:  IP ... ESP IV payload
	 */
	if (M_LEADINGSPACE(md) < esphlen || (md->m_flags & M_EXT) != 0) {
		MGET(n, M_DONTWAIT, MT_DATA);
		if (!n) {
			m_freem(m);
			error = ENOBUFS;
			goto fail;
		}
		n->m_len = esphlen;
		mprev->m_next = n;
		n->m_next = md;
		m->m_pkthdr.len += esphlen;
		esp = mtod(n, struct esp *);
	} else {
		md->m_len += esphlen;
		md->m_data -= esphlen;
		m->m_pkthdr.len += esphlen;
		esp = mtod(md, struct esp *);
	}

	nxt = *nexthdrp;
	*nexthdrp = IPPROTO_ESP;
	switch (af) {
#ifdef INET
	case AF_INET:
		if (esphlen < (IP_MAXPACKET - ntohs(ip->ip_len)))
			ip->ip_len = htons(ntohs(ip->ip_len) + esphlen);
		else {
			ipseclog((LOG_ERR,
			    "IPv4 ESP output: size exceeds limit\n"));
			ipsecstat.out_inval++;
			m_freem(m);
			error = EMSGSIZE;
			goto fail;
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		/* total packet length will be computed in ip6_output() */
		break;
#endif
	}
    }

	/* initialize esp header. */
	esp->esp_spi = spi;
	if ((sav->flags & SADB_X_EXT_OLD) == 0) {
		struct newesp *nesp;
		nesp = (struct newesp *)esp;
		if (sav->replay->count == ~0) {
			if ((sav->flags & SADB_X_EXT_CYCSEQ) == 0) {
				/* XXX Is it noisy ? */
				ipseclog((LOG_WARNING,
				    "replay counter overflowed. %s\n",
				    ipsec_logsastr(sav)));
				stat->out_inval++;
				m_freem(m);
				return EINVAL;
			}
		}
		sav->replay->count++;
		/*
		 * XXX sequence number must not be cycled, if the SA is
		 * installed by IKE daemon.
		 */
		nesp->esp_seq = htonl(sav->replay->count & 0xffffffff);
	}

    {
	/*
	 * find the last mbuf. make some room for ESP trailer.
	 */
#ifdef INET
	struct ip *ip = NULL;
#endif
	size_t padbound;
	u_char *extend;
	int i;
	int randpadmax;

	if (algo->padbound)
		padbound = algo->padbound;
	else
		padbound = 4;
	/* ESP packet, including nxthdr field, must be length of 4n */
	if (padbound < 4)
		padbound = 4;

	extendsiz = padbound - (plen % padbound);
	if (extendsiz == 1)
		extendsiz = padbound + 1;

	/* random padding */
	switch (af) {
#ifdef INET
	case AF_INET:
		randpadmax = ip4_esp_randpad;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		randpadmax = ip6_esp_randpad;
		break;
#endif
	default:
		randpadmax = -1;
		break;
	}
	if (randpadmax < 0 || plen + extendsiz >= randpadmax)
		;
	else {
		int n;

		/* round */
		randpadmax = (randpadmax / padbound) * padbound;
		n = (randpadmax - plen + extendsiz) / padbound;

		if (n > 0)
			n = (random() % n) * padbound;
		else
			n = 0;

		/*
		 * make sure we do not pad too much.
		 * MLEN limitation comes from the trailer attachment
		 * code below.
		 * 256 limitation comes from sequential padding.
		 * also, the 1-octet length field in ESP trailer imposes
		 * limitation (but is less strict than sequential padding
		 * as length field do not count the last 2 octets).
		 */
		if (extendsiz + n <= MLEN && extendsiz + n < 256)
			extendsiz += n;
	}

#ifdef DIAGNOSTIC
	if (extendsiz > MLEN || extendsiz >= 256)
		panic("extendsiz too big in esp_output");
#endif

	n = m;
	while (n->m_next)
		n = n->m_next;

	/*
	 * if M_EXT, the external mbuf data may be shared among
	 * two consequtive TCP packets, and it may be unsafe to use the
	 * trailing space.
	 */
	if (!(n->m_flags & M_EXT) && extendsiz < M_TRAILINGSPACE(n)) {
		extend = mtod(n, u_char *) + n->m_len;
		n->m_len += extendsiz;
		m->m_pkthdr.len += extendsiz;
	} else {
		struct mbuf *nn;

		MGET(nn, M_DONTWAIT, MT_DATA);
		if (!nn) {
			ipseclog((LOG_DEBUG, "esp%d_output: can't alloc mbuf",
			    afnumber));
			m_freem(m);
			error = ENOBUFS;
			goto fail;
		}
		extend = mtod(nn, u_char *);
		nn->m_len = extendsiz;
		nn->m_next = NULL;
		n->m_next = nn;
		n = nn;
		m->m_pkthdr.len += extendsiz;
	}
	switch (sav->flags & SADB_X_EXT_PMASK) {
	case SADB_X_EXT_PRAND:
		key_randomfill(extend, extendsiz);
		break;
	case SADB_X_EXT_PZERO:
		bzero(extend, extendsiz);
		break;
	case SADB_X_EXT_PSEQ:
		for (i = 0; i < extendsiz; i++)
			extend[i] = (i + 1) & 0xff;
		break;
	}

	/* initialize esp trailer. */
	esptail = (struct esptail *)
		(mtod(n, u_int8_t *) + n->m_len - sizeof(struct esptail));
	esptail->esp_nxt = nxt;
	esptail->esp_padlen = extendsiz - 2;

	/* modify IP header (for ESP header part only) */
	switch (af) {
#ifdef INET
	case AF_INET:
		ip = mtod(m, struct ip *);
		if (extendsiz < (IP_MAXPACKET - ntohs(ip->ip_len)))
			ip->ip_len = htons(ntohs(ip->ip_len) + extendsiz);
		else {
			ipseclog((LOG_ERR,
			    "IPv4 ESP output: size exceeds limit\n"));
			ipsecstat.out_inval++;
			m_freem(m);
			error = EMSGSIZE;
			goto fail;
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		/* total packet length will be computed in ip6_output() */
		break;
#endif
	}
    }

	/*
	 * pre-compute and cache intermediate key
	 */
	error = esp_schedule(algo, sav);
	if (error) {
		m_freem(m);
		stat->out_inval++;
		goto fail;
	}

	/*
	 * encrypt the packet, based on security association
	 * and the algorithm specified.
	 */
	if (!algo->encrypt)
		panic("internal error: no encrypt function");
	if ((*algo->encrypt)(m, espoff, plen + extendsiz, sav, algo, ivlen)) {
		/* m is already freed */
		ipseclog((LOG_ERR, "packet encryption failure\n"));
		stat->out_inval++;
		error = EINVAL;
		goto fail;
	}

	/*
	 * calculate ICV if required.
	 */
	if (!sav->replay)
		goto noantireplay;
	if (!sav->key_auth)
		goto noantireplay;
	if (sav->alg_auth == SADB_AALG_NONE)
		goto noantireplay;

    {
	const struct ah_algorithm *aalgo;
	u_char authbuf[AH_MAXSUMSIZE];
	struct mbuf *n;
	u_char *p;
	size_t siz;
#ifdef INET
	struct ip *ip;
#endif

	aalgo = ah_algorithm_lookup(sav->alg_auth);
	if (!aalgo)
		goto noantireplay;
	siz = ((aalgo->sumsiz)(sav) + 3) & ~(4 - 1);
	if (AH_MAXSUMSIZE < siz)
		panic("assertion failed for AH_MAXSUMSIZE");

	if (esp_auth(m, espoff, m->m_pkthdr.len - espoff, sav, authbuf)) {
		ipseclog((LOG_ERR, "ESP checksum generation failure\n"));
		m_freem(m);
		error = EINVAL;
		stat->out_inval++;
		goto fail;
	}

	n = m;
	while (n->m_next)
		n = n->m_next;

	if (!(n->m_flags & M_EXT) && siz < M_TRAILINGSPACE(n)) { /* XXX */
		n->m_len += siz;
		m->m_pkthdr.len += siz;
		p = mtod(n, u_char *) + n->m_len - siz;
	} else {
		struct mbuf *nn;

		MGET(nn, M_DONTWAIT, MT_DATA);
		if (!nn) {
			ipseclog((LOG_DEBUG, "can't alloc mbuf in esp%d_output",
			    afnumber));
			m_freem(m);
			error = ENOBUFS;
			goto fail;
		}
		nn->m_len = siz;
		nn->m_next = NULL;
		n->m_next = nn;
		n = nn;
		m->m_pkthdr.len += siz;
		p = mtod(nn, u_char *);
	}
	bcopy(authbuf, p, siz);

	/* modify IP header (for ESP header part only) */
	switch (af) {
#ifdef INET
	case AF_INET:
		ip = mtod(m, struct ip *);
		if (siz < (IP_MAXPACKET - ntohs(ip->ip_len)))
			ip->ip_len = htons(ntohs(ip->ip_len) + siz);
		else {
			ipseclog((LOG_ERR,
			    "IPv4 ESP output: size exceeds limit\n"));
			ipsecstat.out_inval++;
			m_freem(m);
			error = EMSGSIZE;
			goto fail;
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		/* total packet length will be computed in ip6_output() */
		break;
#endif
	}
    }

noantireplay:
	if (!m) {
		ipseclog((LOG_ERR,
		    "NULL mbuf after encryption in esp%d_output", afnumber));
	} else
		stat->out_success++;
	stat->out_esphist[sav->alg_enc]++;
	key_sa_recordxfer(sav, m);
	return 0;

fail:
#if 1
	return error;
#else
	panic("something bad in esp_output");
#endif
}

#ifdef INET
int
esp4_output(m, isr)
	struct mbuf *m;
	struct ipsecrequest *isr;
{
	struct ip *ip;
	if (m->m_len < sizeof(struct ip)) {
		ipseclog((LOG_DEBUG, "esp4_output: first mbuf too short\n"));
		m_freem(m);
		return 0;
	}
	ip = mtod(m, struct ip *);
	/* XXX assumes that m->m_next points to payload */
	return esp_output(m, &ip->ip_p, m->m_next, isr, AF_INET);
}
#endif /* INET */

#ifdef INET6
int
esp6_output(m, nexthdrp, md, isr)
	struct mbuf *m;
	u_char *nexthdrp;
	struct mbuf *md;
	struct ipsecrequest *isr;
{
	if (m->m_len < sizeof(struct ip6_hdr)) {
		ipseclog((LOG_DEBUG, "esp6_output: first mbuf too short\n"));
		m_freem(m);
		return 0;
	}
	return esp_output(m, nexthdrp, md, isr, AF_INET6);
}
#endif /* INET6 */
