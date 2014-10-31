/*	$NetBSD: getnameinfo.c,v 1.6 2007/07/22 05:19:01 lukem Exp $	*/
/*	from	?	*/

/*
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
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - Return values.  There seems to be no standard for return value (RFC2553)
 *   but INRIA implementation returns EAI_xxx defined for getaddrinfo().
 * - RFC2553 says that we should raise error on short buffer.  X/Open says
 *   we need to truncate the result.  We obey RFC2553 (and X/Open should be
 *   modified).
 * - What is "local" in NI_FQDN?
 * - NI_NAMEREQD and NI_NUMERICHOST conflict with each other.
 * - (KAME extension) NI_WITHSCOPEID when called with global address,
 *   and sin6_scope_id filled
 */

#include "tnftp.h"

#define SUCCESS 0
#define ANY 0
#define YES 1
#define NO  0

static struct afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
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
	unsigned char	si_len;
	unsigned char	si_family;
	unsigned short	si_port;
};

#ifdef INET6
static int ip6_parsenumeric(const struct sockaddr *, const char *, char *,
				 size_t, int);
static int ip6_sa2str(const struct sockaddr_in6 *, char *, size_t, int);
#endif

#define ENI_NOSOCKET 	EAI_FAIL		/*XXX*/
#define ENI_NOSERVNAME	EAI_NONAME
#define ENI_NOHOSTNAME	EAI_NONAME
#define ENI_MEMORY	EAI_MEMORY
#define ENI_SYSTEM	EAI_SYSTEM
#define ENI_FAMILY	EAI_FAMILY
#define ENI_SALEN	EAI_FAMILY

int
getnameinfo(const struct sockaddr *sa, socklen_t salen,
    char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct afd *afd;
	struct servent *sp;
	struct hostent *hp;
	unsigned short port;
	int family, i;
	const char *addr;
	unsigned int v4a;
	char numserv[512];
	char numaddr[512];

	if (sa == NULL)
		return ENI_NOSOCKET;

#if defined(HAVE_STRUCT_SOCKADDR_SA_LEN)
	if (sa->sa_len != salen)
		return ENI_SALEN;
#endif
	
	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return ENI_FAMILY;
	
 found:
	if (salen != afd->a_socklen)
		return ENI_SALEN;
	
	/* network byte order */
	port = ((const struct sockinet *)sa)->si_port;
	addr = (const char *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: RFC2553 says that serv == NULL OR servlen == 0
		 * means that the caller does not want the result.
		 */
	} else {
		if (flags & NI_NUMERICSERV)
			sp = NULL;
		else {
			sp = getservbyport(port,
				(flags & NI_DGRAM) ? "udp" : "tcp");
		}
		if (sp) {
			if (strlen(sp->s_name) > servlen)
				return ENI_MEMORY;
			strcpy(serv, sp->s_name);
		} else {
			snprintf(numserv, sizeof(numserv), "%d", ntohs(port));
			if (strlen(numserv) > servlen)
				return ENI_MEMORY;
			strcpy(serv, numserv);
		}
	}

	switch (sa->sa_family) {
	case AF_INET:
		v4a = (unsigned int)
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
		 * "||" here: RFC2553 says that host == NULL OR hostlen == 0
		 * means that the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICHOST) {
		int numaddrlen;

		/* NUMERICHOST and NAMEREQD conflicts with each other */
		if (flags & NI_NAMEREQD)
			return ENI_NOHOSTNAME;

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
				return ENI_SYSTEM;
			numaddrlen = strlen(numaddr);
			if (numaddrlen + 1 > hostlen) /* don't forget terminator */
				return ENI_MEMORY;
			strcpy(host, numaddr);
			break;
		}
	} else {
		hp = gethostbyaddr(addr, afd->a_addrlen, afd->a_af);

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
			if (strlen(hp->h_name) > hostlen) {
				return ENI_MEMORY;
			}
			strcpy(host, hp->h_name);
		} else {
			if (flags & NI_NAMEREQD)
				return ENI_NOHOSTNAME;
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
					return ENI_SYSTEM;
				break;
			}
		}
	}
	return SUCCESS;
}

#ifdef INET6
static int
ip6_parsenumeric(const struct sockaddr *sa, const char *addr, char *host,
	size_t hostlen, int flags)
{
	int numaddrlen;
	char numaddr[512];

	if (inet_ntop(AF_INET6, addr, numaddr, sizeof(numaddr))
	    == NULL)
		return ENI_SYSTEM;

	numaddrlen = strlen(numaddr);
	if (numaddrlen + 1 > hostlen) /* don't forget terminator */
		return ENI_MEMORY;
	strcpy(host, numaddr);

#ifdef NI_WITHSCOPEID
	if (((const struct sockaddr_in6 *)sa)->sin6_scope_id) {
		if (flags & NI_WITHSCOPEID)
		{
			char scopebuf[MAXHOSTNAMELEN];
			int scopelen;

			/* ip6_sa2str never fails */
			scopelen = ip6_sa2str((const struct sockaddr_in6 *)sa,
					      scopebuf, sizeof(scopebuf),
					      0);
			if (scopelen + 1 + numaddrlen + 1 > hostlen)
				return ENI_MEMORY;
			/*
			 * construct <numeric-addr><delim><scopeid>
			 */
			memcpy(host + numaddrlen + 1, scopebuf,
			       scopelen);
			host[numaddrlen] = SCOPE_DELIMITER;
			host[numaddrlen + 1 + scopelen] = '\0';
		}
	}
#endif /* NI_WITHSCOPEID */

	return 0;
}

/* ARGSUSED */
static int
ip6_sa2str(const struct sockaddr_in6 *sa6, char *buf, size_t bufsiz, int flags)
{
	unsigned int ifindex = (unsigned int)sa6->sin6_scope_id;
	const struct in6_addr *a6 = &sa6->sin6_addr;

#ifdef notyet
	if (flags & NI_NUMERICSCOPE) {
		return(snprintf(buf, bufsiz, "%d", sa6->sin6_scope_id));
	}
#endif

		/*
		 * XXXLUKEM:	some systems (MacOS X) don't have IF_NAMESIZE
		 *		or if_indextoname()
		 */
#if 0
	/* if_indextoname() does not take buffer size.  not a good api... */
	if ((IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6)) &&
	    bufsiz >= IF_NAMESIZE) {
		char *p = if_indextoname(ifindex, buf);
		if (p) {
			return(strlen(p));
		}
	}
#endif

	/* last resort */
	return(snprintf(buf, bufsiz, "%u", sa6->sin6_scope_id));
}
#endif /* INET6 */
