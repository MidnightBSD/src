/*	$KAME: getnameinfo.c,v 1.61 2002/06/27 09:25:47 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * Copyright (c) 2000 Ben Harris.
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
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - RFC2553 says that we should raise error on short buffer.  X/Open says
 *   we need to truncate the result.  We obey RFC2553 (and X/Open should be
 *   modified).  ipngwg rough consensus seems to follow RFC2553.
 * - What is "local" in NI_FQDN?
 * - NI_NAMEREQD and NI_NUMERICHOST conflict with each other.
 * - (KAME extension) always attach textual scopeid (fe80::1%lo0), if
 *   sin6_scope_id is filled - standardization status?
 *   XXX breaks backward compat for code that expects no scopeid.
 *   beware on merge.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/net/getnameinfo.c,v 1.20 2007/02/28 21:18:38 bms Exp $");

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

static int	getnameinfo_inet(const struct sockaddr *, socklen_t, char *,
    size_t, char *, size_t, int);
#ifdef INET6
static int ip6_parsenumeric(const struct sockaddr *, const char *, char *,
    size_t, int);
static int ip6_sa2str(const struct sockaddr_in6 *, char *, size_t, int);
#endif
static int	getnameinfo_link(const struct sockaddr *, socklen_t, char *,
    size_t, char *, size_t, int);
static int	hexname(const u_int8_t *, size_t, char *, size_t);

int
getnameinfo(const struct sockaddr *sa, socklen_t salen,
    char *host, size_t hostlen, char *serv, size_t servlen,
    int flags)
{

	switch (sa->sa_family) {
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
		return getnameinfo_inet(sa, salen, host, hostlen, serv,
		    servlen, flags);
	case AF_LINK:
		return getnameinfo_link(sa, salen, host, hostlen, serv,
		    servlen, flags);
	default:
		return EAI_FAMILY;
	}
}

static const struct afd {
	int a_af;
	size_t a_addrlen;
	socklen_t a_socklen;
	int a_off;
} afdl [] = {
#ifdef INET6
	{PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
		offsetof(struct sockaddr_in6, sin6_addr)},
#endif
	{PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
		offsetof(struct sockaddr_in, sin_addr)},
	{0, 0, 0},
};

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

static int
getnameinfo_inet(const struct sockaddr *sa, socklen_t salen,
    char *host, size_t hostlen, char *serv, size_t servlen,
    int flags)
{
	const struct afd *afd;
	struct servent *sp;
	struct hostent *hp;
	u_short port;
	int family, i;
	const char *addr;
	u_int32_t v4a;
	int h_error;
	char numserv[512];
	char numaddr[512];

	if (sa == NULL)
		return EAI_FAIL;

	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return EAI_FAMILY;

 found:
	if (salen != afd->a_socklen)
		return EAI_FAIL;

	/* network byte order */
	port = ((const struct sockinet *)sa)->si_port;
	addr = (const char *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: rfc2553bis-03 says that serv == NULL OR
		 * servlen == 0 means that the caller does not want the result.
		 */
	} else {
		if (flags & NI_NUMERICSERV)
			sp = NULL;
		else {
			sp = getservbyport(port,
				(flags & NI_DGRAM) ? "udp" : "tcp");
		}
		if (sp) {
			if (strlen(sp->s_name) + 1 > servlen)
				return EAI_MEMORY;
			strlcpy(serv, sp->s_name, servlen);
		} else {
			snprintf(numserv, sizeof(numserv), "%u", ntohs(port));
			if (strlen(numserv) + 1 > servlen)
				return EAI_MEMORY;
			strlcpy(serv, numserv, servlen);
		}
	}

	switch (sa->sa_family) {
	case AF_INET:
		v4a = (u_int32_t)
		    ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
		if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
			flags |= NI_NUMERICHOST;
		v4a >>= IN_CLASSA_NSHIFT;
		if (v4a == 0)
			flags |= NI_NUMERICHOST;
		break;
#ifdef INET6
	case AF_INET6:
	    {
		const struct sockaddr_in6 *sin6;
		sin6 = (const struct sockaddr_in6 *)sa;
		switch (sin6->sin6_addr.s6_addr[0]) {
		case 0x00:
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
				;
			else if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
				;
			else
				flags |= NI_NUMERICHOST;
			break;
		default:
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				flags |= NI_NUMERICHOST;
			}
			else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
				flags |= NI_NUMERICHOST;
			break;
		}
	    }
		break;
#endif
	}
	if (host == NULL || hostlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: rfc2553bis-03 says that host == NULL or
		 * hostlen == 0 means that the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICHOST) {
		size_t numaddrlen;

		/* NUMERICHOST and NAMEREQD conflicts with each other */
		if (flags & NI_NAMEREQD)
			return EAI_NONAME;

		switch(afd->a_af) {
#ifdef INET6
		case AF_INET6:
		{
			int error;

			if ((error = ip6_parsenumeric(sa, addr, host,
						      hostlen, flags)) != 0)
				return(error);
			break;
		}
#endif
		default:
			if (inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
			    == NULL)
				return EAI_SYSTEM;
			numaddrlen = strlen(numaddr);
			if (numaddrlen + 1 > hostlen) /* don't forget terminator */
				return EAI_MEMORY;
			strlcpy(host, numaddr, hostlen);
			break;
		}
	} else {
		hp = getipnodebyaddr(addr, afd->a_addrlen, afd->a_af, &h_error);

		if (hp) {
#if 0
			/*
			 * commented out, since "for local host" is not
			 * implemented here - see RFC2553 p30
			 */
			if (flags & NI_NOFQDN) {
				char *p;
				p = strchr(hp->h_name, '.');
				if (p)
					*p = '\0';
			}
#endif
			if (strlen(hp->h_name) + 1 > hostlen) {
				freehostent(hp);
				return EAI_MEMORY;
			}
			strlcpy(host, hp->h_name, hostlen);
			freehostent(hp);
		} else {
			if (flags & NI_NAMEREQD)
				return EAI_NONAME;
			switch(afd->a_af) {
#ifdef INET6
			case AF_INET6:
			{
				int error;

				if ((error = ip6_parsenumeric(sa, addr, host,
							      hostlen,
							      flags)) != 0)
					return(error);
				break;
			}
#endif
			default:
				if (inet_ntop(afd->a_af, addr, host,
				    hostlen) == NULL)
					return EAI_SYSTEM;
				break;
			}
		}
	}
	return(0);
}

#ifdef INET6
static int
ip6_parsenumeric(const struct sockaddr *sa, const char *addr,
    char *host, size_t hostlen, int flags)
{
	size_t numaddrlen;
	char numaddr[512];

	if (inet_ntop(AF_INET6, addr, numaddr, sizeof(numaddr)) == NULL)
		return EAI_SYSTEM;

	numaddrlen = strlen(numaddr);
	if (numaddrlen + 1 > hostlen) /* don't forget terminator */
		return EAI_OVERFLOW;
	strlcpy(host, numaddr, hostlen);

	if (((const struct sockaddr_in6 *)sa)->sin6_scope_id) {
		char zonebuf[MAXHOSTNAMELEN];
		int zonelen;

		zonelen = ip6_sa2str(
		    (const struct sockaddr_in6 *)(const void *)sa,
		    zonebuf, sizeof(zonebuf), flags);
		if (zonelen < 0)
			return EAI_OVERFLOW;
		if (zonelen + 1 + numaddrlen + 1 > hostlen)
			return EAI_OVERFLOW;

		/* construct <numeric-addr><delim><zoneid> */
		memcpy(host + numaddrlen + 1, zonebuf,
		    (size_t)zonelen);
		host[numaddrlen] = SCOPE_DELIMITER;
		host[numaddrlen + 1 + zonelen] = '\0';
	}

	return 0;
}

/* ARGSUSED */
static int
ip6_sa2str(const struct sockaddr_in6 *sa6, char *buf, size_t bufsiz, int flags)
{
	unsigned int ifindex;
	const struct in6_addr *a6;
	int n;

	ifindex = (unsigned int)sa6->sin6_scope_id;
	a6 = &sa6->sin6_addr;

#ifdef NI_NUMERICSCOPE
	if ((flags & NI_NUMERICSCOPE) != 0) {
		n = snprintf(buf, bufsiz, "%u", sa6->sin6_scope_id);
		if (n < 0 || n >= bufsiz)
			return -1;
		else
			return n;
	}
#endif

	/* if_indextoname() does not take buffer size.  not a good api... */
	if ((IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6) ||
	     IN6_IS_ADDR_MC_NODELOCAL(a6)) && bufsiz >= IF_NAMESIZE) {
		char *p = if_indextoname(ifindex, buf);
		if (p) {
			return(strlen(p));
		}
	}

	/* last resort */
	n = snprintf(buf, bufsiz, "%u", sa6->sin6_scope_id);
	if (n < 0 || (size_t)n >= bufsiz)
		return -1;
	else
		return n;
}
#endif /* INET6 */

/*
 * getnameinfo_link():
 * Format a link-layer address into a printable format, paying attention to
 * the interface type.
 */
/* ARGSUSED */
static int
getnameinfo_link(const struct sockaddr *sa, socklen_t salen,
    char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	const struct sockaddr_dl *sdl =
	    (const struct sockaddr_dl *)(const void *)sa;
	int n;

	if (serv != NULL && servlen > 0)
		*serv = '\0';

	if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 && sdl->sdl_slen == 0) {
		n = snprintf(host, hostlen, "link#%d", sdl->sdl_index);
		if (n > hostlen) {
			*host = '\0';
			return EAI_MEMORY;
		}
		return 0;
	}

	switch (sdl->sdl_type) {
	/*
	 * The following have zero-length addresses.
	 * IFT_ATM	(net/if_atmsubr.c)
	 * IFT_FAITH	(net/if_faith.c)
	 * IFT_GIF	(net/if_gif.c)
	 * IFT_LOOP	(net/if_loop.c)
	 * IFT_PPP	(net/if_ppp.c, net/if_spppsubr.c)
	 * IFT_SLIP	(net/if_sl.c, net/if_strip.c)
	 * IFT_STF	(net/if_stf.c)
	 * IFT_L2VLAN	(net/if_vlan.c)
	 * IFT_BRIDGE (net/if_bridge.h>
	 */
	/*
	 * The following use IPv4 addresses as link-layer addresses:
	 * IFT_OTHER	(net/if_gre.c)
	 * IFT_OTHER	(netinet/ip_ipip.c)
	 */
	/* default below is believed correct for all these. */
	case IFT_ARCNET:
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_HIPPI:
	case IFT_ISO88025:
	default:
		return hexname((u_int8_t *)LLADDR(sdl), (size_t)sdl->sdl_alen,
		    host, hostlen);
	}
}

static int
hexname(cp, len, host, hostlen)
	const u_int8_t *cp;
	char *host;
	size_t len, hostlen;
{
	int i, n;
	char *outp = host;

	*outp = '\0';
	for (i = 0; i < len; i++) {
		n = snprintf(outp, hostlen, "%s%02x",
		    i ? ":" : "", cp[i]);
		if (n < 0 || n >= hostlen) {
			*host = '\0';
			return EAI_MEMORY;
		}
		outp += n;
		hostlen -= n;
	}
	return 0;
}
