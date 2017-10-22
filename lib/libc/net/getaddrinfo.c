/*	$KAME: getaddrinfo.c,v 1.15 2000/07/09 04:37:24 itojun Exp $	*/

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
 * "#ifdef FAITH" part is local hack for supporting IPv4-v6 translator.
 *
 * Issues to be discussed:
 * - Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2553 is silent about which error
 *   code must be returned for which situation.
 * - freeaddrinfo(NULL).  RFC2553 is silent about it.  XNET 5.2 says it is
 *   invalid.  current code - SEGV on freeaddrinfo(NULL)
 *
 * Note:
 * - The code filters out AFs that are not supported by the kernel,
 *   when globbing NULL hostname (to loopback, or wildcard).  Is it the right
 *   thing to do?  What is the relationship with post-RFC2553 AI_ADDRCONFIG
 *   in ai_flags?
 * - (post-2553) semantics of AI_ADDRCONFIG itself is too vague.
 *   (1) what should we do against numeric hostname (2) what should we do
 *   against NULL hostname (3) what is AI_ADDRCONFIG itself.  AF not ready?
 *   non-loopback address configured?  global address configured?
 *
 * OS specific notes for freebsd4:
 * - FreeBSD supported $GAI.  The code does not.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/queue.h>
#ifdef INET6
#include <net/if_var.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <netinet6/in6_var.h>	/* XXX */
#endif
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <netdb.h>
#include <resolv.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "res_config.h"

#ifdef DEBUG
#include <syslog.h>
#endif

#include <stdarg.h>
#include <nsswitch.h>
#include "un-namespace.h"
#include "libc_private.h"
#ifdef NS_CACHING
#include "nscache.h"
#endif

#if defined(__KAME__) && defined(INET6)
# define FAITH
#endif

#define ANY 0
#define YES 1
#define NO  0

static const char in_addrany[] = { 0, 0, 0, 0 };
static const char in_loopback[] = { 127, 0, 0, 1 };
#ifdef INET6
static const char in6_addrany[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const char in6_loopback[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};
#endif

struct policyqueue {
	TAILQ_ENTRY(policyqueue) pc_entry;
#ifdef INET6
	struct in6_addrpolicy pc_policy;
#endif
};
TAILQ_HEAD(policyhead, policyqueue);

static const struct afd {
	int a_af;
	int a_addrlen;
	socklen_t a_socklen;
	int a_off;
	const char *a_addrany;
	const char *a_loopback;
	int a_scoped;
} afdl [] = {
#ifdef INET6
#define	N_INET6 0
	{PF_INET6, sizeof(struct in6_addr),
	 sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr),
	 in6_addrany, in6_loopback, 1},
#define	N_INET 1
#else
#define	N_INET 0
#endif
	{PF_INET, sizeof(struct in_addr),
	 sizeof(struct sockaddr_in),
	 offsetof(struct sockaddr_in, sin_addr),
	 in_addrany, in_loopback, 0},
	{0, 0, 0, 0, NULL, NULL, 0},
};

struct explore {
	int e_af;
	int e_socktype;
	int e_protocol;
	const char *e_protostr;
	int e_wild;
#define WILD_AF(ex)		((ex)->e_wild & 0x01)
#define WILD_SOCKTYPE(ex)	((ex)->e_wild & 0x02)
#define WILD_PROTOCOL(ex)	((ex)->e_wild & 0x04)
};

static const struct explore explore[] = {
#if 0
	{ PF_LOCAL, ANY, ANY, NULL, 0x01 },
#endif
#ifdef INET6
	{ PF_INET6, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_INET6, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_INET6, SOCK_STREAM, IPPROTO_SCTP, "sctp", 0x03 },
	{ PF_INET6, SOCK_SEQPACKET, IPPROTO_SCTP, "sctp", 0x07 },
	{ PF_INET6, SOCK_RAW, ANY, NULL, 0x05 },
#endif
	{ PF_INET, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_INET, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_INET, SOCK_STREAM, IPPROTO_SCTP, "sctp", 0x03 },
	{ PF_INET, SOCK_SEQPACKET, IPPROTO_SCTP, "sctp", 0x07 },
	{ PF_INET, SOCK_RAW, ANY, NULL, 0x05 },
	{ -1, 0, 0, NULL, 0 },
};

#ifdef INET6
#define PTON_MAX	16
#else
#define PTON_MAX	4
#endif

#define AIO_SRCFLAG_DEPRECATED	0x1

struct ai_order {
	union {
		struct sockaddr_storage aiou_ss;
		struct sockaddr aiou_sa;
	} aio_src_un;
#define aio_srcsa aio_src_un.aiou_sa
	u_int32_t aio_srcflag;
	int aio_srcscope;
	int aio_dstscope;
	struct policyqueue *aio_srcpolicy;
	struct policyqueue *aio_dstpolicy;
	struct addrinfo *aio_ai;
	int aio_matchlen;
};

static const ns_src default_dns_files[] = {
	{ NSSRC_FILES, 	NS_SUCCESS },
	{ NSSRC_DNS, 	NS_SUCCESS },
	{ 0 }
};

struct res_target {
	struct res_target *next;
	const char *name;	/* domain name */
	int qclass, qtype;	/* class and type of query */
	u_char *answer;		/* buffer to put answer */
	int anslen;		/* size of answer buffer */
	int n;			/* result length */
};

#define MAXPACKET	(64*1024)

typedef union {
	HEADER hdr;
	u_char buf[MAXPACKET];
} querybuf;

static int str2number(const char *, int *);
static int explore_copy(const struct addrinfo *, const struct addrinfo *,
	struct addrinfo **);
static int explore_null(const struct addrinfo *,
	const char *, struct addrinfo **);
static int explore_numeric(const struct addrinfo *, const char *,
	const char *, struct addrinfo **, const char *);
static int explore_numeric_scope(const struct addrinfo *, const char *,
	const char *, struct addrinfo **);
static int get_canonname(const struct addrinfo *,
	struct addrinfo *, const char *);
static struct addrinfo *get_ai(const struct addrinfo *,
	const struct afd *, const char *);
static struct addrinfo *copy_ai(const struct addrinfo *);
static int get_portmatch(const struct addrinfo *, const char *);
static int get_port(struct addrinfo *, const char *, int);
static const struct afd *find_afd(int);
static int addrconfig(struct addrinfo *);
static void set_source(struct ai_order *, struct policyhead *);
static int comp_dst(const void *, const void *);
#ifdef INET6
static int ip6_str2scopeid(char *, struct sockaddr_in6 *, u_int32_t *);
#endif
static int gai_addr2scopetype(struct sockaddr *);

static int explore_fqdn(const struct addrinfo *, const char *,
	const char *, struct addrinfo **);

static int reorder(struct addrinfo *);
static int get_addrselectpolicy(struct policyhead *);
static void free_addrselectpolicy(struct policyhead *);
static struct policyqueue *match_addrselectpolicy(struct sockaddr *,
	struct policyhead *);
static int matchlen(struct sockaddr *, struct sockaddr *);

static struct addrinfo *getanswer(const querybuf *, int, const char *, int,
	const struct addrinfo *, res_state);
#if defined(RESOLVSORT)
static int addr4sort(struct addrinfo *, res_state);
#endif
static int _dns_getaddrinfo(void *, void *, va_list);
static void _sethtent(FILE **);
static void _endhtent(FILE **);
static struct addrinfo *_gethtent(FILE **, const char *,
	const struct addrinfo *);
static int _files_getaddrinfo(void *, void *, va_list);
#ifdef YP
static struct addrinfo *_yphostent(char *, const struct addrinfo *);
static int _yp_getaddrinfo(void *, void *, va_list);
#endif
#ifdef NS_CACHING
static int addrinfo_id_func(char *, size_t *, va_list, void *);
static int addrinfo_marshal_func(char *, size_t *, void *, va_list, void *);
static int addrinfo_unmarshal_func(char *, size_t, void *, va_list, void *);
#endif

static int res_queryN(const char *, struct res_target *, res_state);
static int res_searchN(const char *, struct res_target *, res_state);
static int res_querydomainN(const char *, const char *,
	struct res_target *, res_state);

/* XXX macros that make external reference is BAD. */

#define GET_AI(ai, afd, addr) \
do { \
	/* external reference: pai, error, and label free */ \
	(ai) = get_ai(pai, (afd), (addr)); \
	if ((ai) == NULL) { \
		error = EAI_MEMORY; \
		goto free; \
	} \
} while (/*CONSTCOND*/0)

#define GET_PORT(ai, serv) \
do { \
	/* external reference: error and label free */ \
	error = get_port((ai), (serv), 0); \
	if (error != 0) \
		goto free; \
} while (/*CONSTCOND*/0)

#define GET_CANONNAME(ai, str) \
do { \
	/* external reference: pai, error and label free */ \
	error = get_canonname(pai, (ai), (str)); \
	if (error != 0) \
		goto free; \
} while (/*CONSTCOND*/0)

#define ERR(err) \
do { \
	/* external reference: error, and label bad */ \
	error = (err); \
	goto bad; \
	/*NOTREACHED*/ \
} while (/*CONSTCOND*/0)

#define MATCH_FAMILY(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == PF_UNSPEC || (y) == PF_UNSPEC)))
#define MATCH(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == ANY || (y) == ANY)))

void
freeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *next;

	do {
		next = ai->ai_next;
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		/* no need to free(ai->ai_addr) */
		free(ai);
		ai = next;
	} while (ai);
}

static int
str2number(const char *p, int *portp)
{
	char *ep;
	unsigned long v;

	if (*p == '\0')
		return -1;
	ep = NULL;
	errno = 0;
	v = strtoul(p, &ep, 10);
	if (errno == 0 && ep && *ep == '\0' && v <= UINT_MAX) {
		*portp = v;
		return 0;
	} else
		return -1;
}

int
getaddrinfo(const char *hostname, const char *servname,
    const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo sentinel;
	struct addrinfo *cur;
	int error = 0;
	struct addrinfo ai, ai0, *afai;
	struct addrinfo *pai;
	const struct afd *afd;
	const struct explore *ex;
	struct addrinfo *afailist[sizeof(afdl)/sizeof(afdl[0])];
	struct addrinfo *afai_unspec;
	int found;
	int numeric = 0;

	/* ensure we return NULL on errors */
	*res = NULL;

	memset(&ai, 0, sizeof(ai));

	memset(afailist, 0, sizeof(afailist));
	afai_unspec = NULL;

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;
	pai = &ai;
	pai->ai_flags = 0;
	pai->ai_family = PF_UNSPEC;
	pai->ai_socktype = ANY;
	pai->ai_protocol = ANY;
	pai->ai_addrlen = 0;
	pai->ai_canonname = NULL;
	pai->ai_addr = NULL;
	pai->ai_next = NULL;

	if (hostname == NULL && servname == NULL)
		return EAI_NONAME;
	if (hints) {
		/* error check for hints */
		if (hints->ai_addrlen || hints->ai_canonname ||
		    hints->ai_addr || hints->ai_next)
			ERR(EAI_BADHINTS); /* xxx */
		if (hints->ai_flags & ~AI_MASK)
			ERR(EAI_BADFLAGS);
		switch (hints->ai_family) {
		case PF_UNSPEC:
		case PF_INET:
#ifdef INET6
		case PF_INET6:
#endif
			break;
		default:
			ERR(EAI_FAMILY);
		}
		memcpy(pai, hints, sizeof(*pai));

		/*
		 * if both socktype/protocol are specified, check if they
		 * are meaningful combination.
		 */
		if (pai->ai_socktype != ANY && pai->ai_protocol != ANY) {
			for (ex = explore; ex->e_af >= 0; ex++) {
				if (!MATCH_FAMILY(pai->ai_family, ex->e_af,
				    WILD_AF(ex)))
					continue;
				if (!MATCH(pai->ai_socktype, ex->e_socktype,
				    WILD_SOCKTYPE(ex)))
					continue;
				if (!MATCH(pai->ai_protocol, ex->e_protocol,
				    WILD_PROTOCOL(ex)))
					continue;

				/* matched */
				break;
			}

			if (ex->e_af < 0)
				ERR(EAI_BADHINTS);
		}
	}

	/*
	 * check for special cases.  (1) numeric servname is disallowed if
	 * socktype/protocol are left unspecified. (2) servname is disallowed
	 * for raw and other inet{,6} sockets.
	 */
	if (MATCH_FAMILY(pai->ai_family, PF_INET, 1)
#ifdef PF_INET6
	    || MATCH_FAMILY(pai->ai_family, PF_INET6, 1)
#endif
	    ) {
		ai0 = *pai;	/* backup *pai */

		if (pai->ai_family == PF_UNSPEC) {
#ifdef PF_INET6
			pai->ai_family = PF_INET6;
#else
			pai->ai_family = PF_INET;
#endif
		}
		error = get_portmatch(pai, servname);
		if (error)
			goto bad;

		*pai = ai0;
	}

	ai0 = *pai;

	/*
	 * NULL hostname, or numeric hostname.
	 * If numeric representation of AF1 can be interpreted as FQDN
	 * representation of AF2, we need to think again about the code below.
	 */
	found = 0;
	for (afd = afdl; afd->a_af; afd++) {
		*pai = ai0;

		if (!MATCH_FAMILY(pai->ai_family, afd->a_af, 1))
			continue;

		if (pai->ai_family == PF_UNSPEC)
			pai->ai_family = afd->a_af;

		if (hostname == NULL) {
			error = explore_null(pai, servname,
			    &afailist[afd - afdl]);

			/*
			 * Errors from explore_null should be unexpected and
			 * be caught to avoid returning an incomplete result.
			 */
			if (error != 0)
				goto bad;
		} else {
			error = explore_numeric_scope(pai, hostname, servname,
			    &afailist[afd - afdl]);

			/*
			 * explore_numeric_scope returns an error for address
			 * families that do not match that of hostname.
			 * Thus we should not catch the error at this moment. 
			 */
		}

		if (!error && afailist[afd - afdl])
			found++;
	}
	if (found) {
		numeric = 1;
		goto globcopy;
	}

	if (hostname == NULL)
		ERR(EAI_NONAME);	/* used to be EAI_NODATA */
	if (pai->ai_flags & AI_NUMERICHOST)
		ERR(EAI_NONAME);

	if ((pai->ai_flags & AI_ADDRCONFIG) != 0 && !addrconfig(&ai0))
		ERR(EAI_FAIL);

	/*
	 * hostname as alphabetical name.
	 */
	*pai = ai0;
	error = explore_fqdn(pai, hostname, servname, &afai_unspec);

globcopy:
	for (ex = explore; ex->e_af >= 0; ex++) {
		*pai = ai0;

		if (!MATCH_FAMILY(pai->ai_family, ex->e_af, WILD_AF(ex)))
			continue;
		if (!MATCH(pai->ai_socktype, ex->e_socktype,
		    WILD_SOCKTYPE(ex)))
			continue;
		if (!MATCH(pai->ai_protocol, ex->e_protocol,
		    WILD_PROTOCOL(ex)))
			continue;

		if (pai->ai_family == PF_UNSPEC)
			pai->ai_family = ex->e_af;
		if (pai->ai_socktype == ANY && ex->e_socktype != ANY)
			pai->ai_socktype = ex->e_socktype;
		if (pai->ai_protocol == ANY && ex->e_protocol != ANY)
			pai->ai_protocol = ex->e_protocol;

		/*
		 * if the servname does not match socktype/protocol, ignore it.
		 */
		if (get_portmatch(pai, servname) != 0)
			continue;

		if (afai_unspec)
			afai = afai_unspec;
		else {
			if ((afd = find_afd(pai->ai_family)) == NULL)
				continue;
			/* XXX assumes that afd points inside afdl[] */
			afai = afailist[afd - afdl];
		}
		if (!afai)
			continue;

		error = explore_copy(pai, afai, &cur->ai_next);
		if (error != 0)
			goto bad;

		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}

	/*
	 * ensure we return either:
	 * - error == 0, non-NULL *res
	 * - error != 0, NULL *res
	 */
	if (error == 0) {
		if (sentinel.ai_next) {
			/*
			 * If the returned entry is for an active connection,
			 * and the given name is not numeric, reorder the
			 * list, so that the application would try the list
			 * in the most efficient order.  Since the head entry
			 * of the original list may contain ai_canonname and
			 * that entry may be moved elsewhere in the new list,
			 * we keep the pointer and will  restore it in the new
			 * head entry.  (Note that RFC3493 requires the head
			 * entry store it when requested by the caller).
			 */
			if (hints == NULL || !(hints->ai_flags & AI_PASSIVE)) {
				if (!numeric) {
					char *canonname;

					canonname =
					    sentinel.ai_next->ai_canonname;
					sentinel.ai_next->ai_canonname = NULL;
					(void)reorder(&sentinel);
					if (sentinel.ai_next->ai_canonname ==
					    NULL) {
						sentinel.ai_next->ai_canonname
						    = canonname;
					} else if (canonname != NULL)
						free(canonname);
				}
			}
			*res = sentinel.ai_next;
		} else
			error = EAI_FAIL;
	}

bad:
	if (afai_unspec)
		freeaddrinfo(afai_unspec);
	for (afd = afdl; afd->a_af; afd++) {
		if (afailist[afd - afdl])
			freeaddrinfo(afailist[afd - afdl]);
	}
	if (!*res)
		if (sentinel.ai_next)
			freeaddrinfo(sentinel.ai_next);

	return (error);
}

static int
reorder(struct addrinfo *sentinel)
{
	struct addrinfo *ai, **aip;
	struct ai_order *aio;
	int i, n;
	struct policyhead policyhead;

	/* count the number of addrinfo elements for sorting. */
	for (n = 0, ai = sentinel->ai_next; ai != NULL; ai = ai->ai_next, n++)
		;

	/*
	 * If the number is small enough, we can skip the reordering process.
	 */
	if (n <= 1)
		return(n);

	/* allocate a temporary array for sort and initialization of it. */
	if ((aio = malloc(sizeof(*aio) * n)) == NULL)
		return(n);	/* give up reordering */
	memset(aio, 0, sizeof(*aio) * n);

	/* retrieve address selection policy from the kernel */
	TAILQ_INIT(&policyhead);
	if (!get_addrselectpolicy(&policyhead)) {
		/* no policy is installed into kernel, we don't sort. */
		free(aio);
		return (n);
	}

	for (i = 0, ai = sentinel->ai_next; i < n; ai = ai->ai_next, i++) {
		aio[i].aio_ai = ai;
		aio[i].aio_dstscope = gai_addr2scopetype(ai->ai_addr);
		aio[i].aio_dstpolicy = match_addrselectpolicy(ai->ai_addr,
							      &policyhead);
		set_source(&aio[i], &policyhead);
	}

	/* perform sorting. */
	qsort(aio, n, sizeof(*aio), comp_dst);

	/* reorder the addrinfo chain. */
	for (i = 0, aip = &sentinel->ai_next; i < n; i++) {
		*aip = aio[i].aio_ai;
		aip = &aio[i].aio_ai->ai_next;
	}
	*aip = NULL;

	/* cleanup and return */
	free(aio);
	free_addrselectpolicy(&policyhead);
	return(n);
}

static int
get_addrselectpolicy(struct policyhead *head)
{
#ifdef INET6
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_ADDRCTLPOLICY };
	size_t l;
	char *buf;
	struct in6_addrpolicy *pol, *ep;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &l, NULL, 0) < 0)
		return (0);
	if (l == 0)
		return (0);
	if ((buf = malloc(l)) == NULL)
		return (0);
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &l, NULL, 0) < 0) {
		free(buf);
		return (0);
	}

	ep = (struct in6_addrpolicy *)(buf + l);
	for (pol = (struct in6_addrpolicy *)buf; pol + 1 <= ep; pol++) {
		struct policyqueue *new;

		if ((new = malloc(sizeof(*new))) == NULL) {
			free_addrselectpolicy(head); /* make the list empty */
			break;
		}
		new->pc_policy = *pol;
		TAILQ_INSERT_TAIL(head, new, pc_entry);
	}

	free(buf);
	return (1);
#else
	return (0);
#endif
}

static void
free_addrselectpolicy(struct policyhead *head)
{
	struct policyqueue *ent, *nent;

	for (ent = TAILQ_FIRST(head); ent; ent = nent) {
		nent = TAILQ_NEXT(ent, pc_entry);
		TAILQ_REMOVE(head, ent, pc_entry);
		free(ent);
	}
}

static struct policyqueue *
match_addrselectpolicy(struct sockaddr *addr, struct policyhead *head)
{
#ifdef INET6
	struct policyqueue *ent, *bestent = NULL;
	struct in6_addrpolicy *pol;
	int matchlen, bestmatchlen = -1;
	u_char *mp, *ep, *k, *p, m;
	struct sockaddr_in6 key;

	switch(addr->sa_family) {
	case AF_INET6:
		key = *(struct sockaddr_in6 *)addr;
		break;
	case AF_INET:
		/* convert the address into IPv4-mapped IPv6 address. */
		memset(&key, 0, sizeof(key));
		key.sin6_family = AF_INET6;
		key.sin6_len = sizeof(key);
		key.sin6_addr.s6_addr[10] = 0xff;
		key.sin6_addr.s6_addr[11] = 0xff;
		memcpy(&key.sin6_addr.s6_addr[12],
		       &((struct sockaddr_in *)addr)->sin_addr, 4);
		break;
	default:
		return(NULL);
	}

	for (ent = TAILQ_FIRST(head); ent; ent = TAILQ_NEXT(ent, pc_entry)) {
		pol = &ent->pc_policy;
		matchlen = 0;

		mp = (u_char *)&pol->addrmask.sin6_addr;
		ep = mp + 16;	/* XXX: scope field? */
		k = (u_char *)&key.sin6_addr;
		p = (u_char *)&pol->addr.sin6_addr;
		for (; mp < ep && *mp; mp++, k++, p++) {
			m = *mp;
			if ((*k & m) != *p)
				goto next; /* not match */
			if (m == 0xff) /* short cut for a typical case */
				matchlen += 8;
			else {
				while (m >= 0x80) {
					matchlen++;
					m <<= 1;
				}
			}
		}

		/* matched.  check if this is better than the current best. */
		if (matchlen > bestmatchlen) {
			bestent = ent;
			bestmatchlen = matchlen;
		}

	  next:
		continue;
	}

	return(bestent);
#else
	return(NULL);
#endif

}

static void
set_source(struct ai_order *aio, struct policyhead *ph)
{
	struct addrinfo ai = *aio->aio_ai;
	struct sockaddr_storage ss;
	socklen_t srclen;
	int s;

	/* set unspec ("no source is available"), just in case */
	aio->aio_srcsa.sa_family = AF_UNSPEC;
	aio->aio_srcscope = -1;

	switch(ai.ai_family) {
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
		break;
	default:		/* ignore unsupported AFs explicitly */
		return;
	}

	/* XXX: make a dummy addrinfo to call connect() */
	ai.ai_socktype = SOCK_DGRAM;
	ai.ai_protocol = IPPROTO_UDP; /* is UDP too specific? */
	ai.ai_next = NULL;
	memset(&ss, 0, sizeof(ss));
	memcpy(&ss, ai.ai_addr, ai.ai_addrlen);
	ai.ai_addr = (struct sockaddr *)&ss;
	get_port(&ai, "1", 0);

	/* open a socket to get the source address for the given dst */
	if ((s = _socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol)) < 0)
		return;		/* give up */
	if (_connect(s, ai.ai_addr, ai.ai_addrlen) < 0)
		goto cleanup;
	srclen = ai.ai_addrlen;
	if (_getsockname(s, &aio->aio_srcsa, &srclen) < 0) {
		aio->aio_srcsa.sa_family = AF_UNSPEC;
		goto cleanup;
	}
	aio->aio_srcscope = gai_addr2scopetype(&aio->aio_srcsa);
	aio->aio_srcpolicy = match_addrselectpolicy(&aio->aio_srcsa, ph);
	aio->aio_matchlen = matchlen(&aio->aio_srcsa, aio->aio_ai->ai_addr);
#ifdef INET6
	if (ai.ai_family == AF_INET6) {
		struct in6_ifreq ifr6;
		u_int32_t flags6;

		memset(&ifr6, 0, sizeof(ifr6));
		memcpy(&ifr6.ifr_addr, ai.ai_addr, ai.ai_addrlen);
		if (_ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) == 0) {
			flags6 = ifr6.ifr_ifru.ifru_flags6;
			if ((flags6 & IN6_IFF_DEPRECATED))
				aio->aio_srcflag |= AIO_SRCFLAG_DEPRECATED;
		}
	}
#endif

  cleanup:
	_close(s);
	return;
}

static int
matchlen(struct sockaddr *src, struct sockaddr *dst)
{
	int match = 0;
	u_char *s, *d;
	u_char *lim, r;
	int addrlen;

	switch (src->sa_family) {
#ifdef INET6
	case AF_INET6:
		s = (u_char *)&((struct sockaddr_in6 *)src)->sin6_addr;
		d = (u_char *)&((struct sockaddr_in6 *)dst)->sin6_addr;
		addrlen = sizeof(struct in6_addr);
		lim = s + addrlen;
		break;
#endif
	case AF_INET:
		s = (u_char *)&((struct sockaddr_in *)src)->sin_addr;
		d = (u_char *)&((struct sockaddr_in *)dst)->sin_addr;
		addrlen = sizeof(struct in_addr);
		lim = s + addrlen;
		break;
	default:
		return(0);
	}

	while (s < lim)
		if ((r = (*d++ ^ *s++)) != 0) {
			while (r < addrlen * 8) {
				match++;
				r <<= 1;
			}
			break;
		} else
			match += 8;
	return(match);
}

static int
comp_dst(const void *arg1, const void *arg2)
{
	const struct ai_order *dst1 = arg1, *dst2 = arg2;

	/*
	 * Rule 1: Avoid unusable destinations.
	 * XXX: we currently do not consider if an appropriate route exists.
	 */
	if (dst1->aio_srcsa.sa_family != AF_UNSPEC &&
	    dst2->aio_srcsa.sa_family == AF_UNSPEC) {
		return(-1);
	}
	if (dst1->aio_srcsa.sa_family == AF_UNSPEC &&
	    dst2->aio_srcsa.sa_family != AF_UNSPEC) {
		return(1);
	}

	/* Rule 2: Prefer matching scope. */
	if (dst1->aio_dstscope == dst1->aio_srcscope &&
	    dst2->aio_dstscope != dst2->aio_srcscope) {
		return(-1);
	}
	if (dst1->aio_dstscope != dst1->aio_srcscope &&
	    dst2->aio_dstscope == dst2->aio_srcscope) {
		return(1);
	}

	/* Rule 3: Avoid deprecated addresses. */
	if (dst1->aio_srcsa.sa_family != AF_UNSPEC &&
	    dst2->aio_srcsa.sa_family != AF_UNSPEC) {
		if (!(dst1->aio_srcflag & AIO_SRCFLAG_DEPRECATED) &&
		    (dst2->aio_srcflag & AIO_SRCFLAG_DEPRECATED)) {
			return(-1);
		}
		if ((dst1->aio_srcflag & AIO_SRCFLAG_DEPRECATED) &&
		    !(dst2->aio_srcflag & AIO_SRCFLAG_DEPRECATED)) {
			return(1);
		}
	}

	/* Rule 4: Prefer home addresses. */
	/* XXX: not implemented yet */

	/* Rule 5: Prefer matching label. */
#ifdef INET6
	if (dst1->aio_srcpolicy && dst1->aio_dstpolicy &&
	    dst1->aio_srcpolicy->pc_policy.label ==
	    dst1->aio_dstpolicy->pc_policy.label &&
	    (dst2->aio_srcpolicy == NULL || dst2->aio_dstpolicy == NULL ||
	     dst2->aio_srcpolicy->pc_policy.label !=
	     dst2->aio_dstpolicy->pc_policy.label)) {
		return(-1);
	}
	if (dst2->aio_srcpolicy && dst2->aio_dstpolicy &&
	    dst2->aio_srcpolicy->pc_policy.label ==
	    dst2->aio_dstpolicy->pc_policy.label &&
	    (dst1->aio_srcpolicy == NULL || dst1->aio_dstpolicy == NULL ||
	     dst1->aio_srcpolicy->pc_policy.label !=
	     dst1->aio_dstpolicy->pc_policy.label)) {
		return(1);
	}
#endif

	/* Rule 6: Prefer higher precedence. */
#ifdef INET6
	if (dst1->aio_dstpolicy &&
	    (dst2->aio_dstpolicy == NULL ||
	     dst1->aio_dstpolicy->pc_policy.preced >
	     dst2->aio_dstpolicy->pc_policy.preced)) {
		return(-1);
	}
	if (dst2->aio_dstpolicy &&
	    (dst1->aio_dstpolicy == NULL ||
	     dst2->aio_dstpolicy->pc_policy.preced >
	     dst1->aio_dstpolicy->pc_policy.preced)) {
		return(1);
	}
#endif

	/* Rule 7: Prefer native transport. */
	/* XXX: not implemented yet */

	/* Rule 8: Prefer smaller scope. */
	if (dst1->aio_dstscope >= 0 &&
	    dst1->aio_dstscope < dst2->aio_dstscope) {
		return(-1);
	}
	if (dst2->aio_dstscope >= 0 &&
	    dst2->aio_dstscope < dst1->aio_dstscope) {
		return(1);
	}

	/*
	 * Rule 9: Use longest matching prefix.
	 * We compare the match length in a same AF only.
	 */
	if (dst1->aio_ai->ai_addr->sa_family ==
	    dst2->aio_ai->ai_addr->sa_family) {
		if (dst1->aio_matchlen > dst2->aio_matchlen) {
			return(-1);
		}
		if (dst1->aio_matchlen < dst2->aio_matchlen) {
			return(1);
		}
	}

	/* Rule 10: Otherwise, leave the order unchanged. */
	return(-1);
}

/*
 * Copy from scope.c.
 * XXX: we should standardize the functions and link them as standard
 * library.
 */
static int
gai_addr2scopetype(struct sockaddr *sa)
{
#ifdef INET6
	struct sockaddr_in6 *sa6;
#endif
	struct sockaddr_in *sa4;

	switch(sa->sa_family) {
#ifdef INET6
	case AF_INET6:
		sa6 = (struct sockaddr_in6 *)sa;
		if (IN6_IS_ADDR_MULTICAST(&sa6->sin6_addr)) {
			/* just use the scope field of the multicast address */
			return(sa6->sin6_addr.s6_addr[2] & 0x0f);
		}
		/*
		 * Unicast addresses: map scope type to corresponding scope
		 * value defined for multcast addresses.
		 * XXX: hardcoded scope type values are bad...
		 */
		if (IN6_IS_ADDR_LOOPBACK(&sa6->sin6_addr))
			return(1); /* node local scope */
		if (IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr))
			return(2); /* link-local scope */
		if (IN6_IS_ADDR_SITELOCAL(&sa6->sin6_addr))
			return(5); /* site-local scope */
		return(14);	/* global scope */
		break;
#endif
	case AF_INET:
		/*
		 * IPv4 pseudo scoping according to RFC 3484.
		 */
		sa4 = (struct sockaddr_in *)sa;
		/* IPv4 autoconfiguration addresses have link-local scope. */
		if (((u_char *)&sa4->sin_addr)[0] == 169 &&
		    ((u_char *)&sa4->sin_addr)[1] == 254)
			return(2);
		/* Private addresses have site-local scope. */
		if (((u_char *)&sa4->sin_addr)[0] == 10 ||
		    (((u_char *)&sa4->sin_addr)[0] == 172 &&
		     (((u_char *)&sa4->sin_addr)[1] & 0xf0) == 16) ||
		    (((u_char *)&sa4->sin_addr)[0] == 192 &&
		     ((u_char *)&sa4->sin_addr)[1] == 168))
			return(14);	/* XXX: It should be 5 unless NAT */
		/* Loopback addresses have link-local scope. */
		if (((u_char *)&sa4->sin_addr)[0] == 127)
			return(2);
		return(14);
		break;
	default:
		errno = EAFNOSUPPORT; /* is this a good error? */
		return(-1);
	}
}

static int
explore_copy(const struct addrinfo *pai, const struct addrinfo *src0,
    struct addrinfo **res)
{
	int error;
	struct addrinfo sentinel, *cur;
	const struct addrinfo *src;

	error = 0;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	for (src = src0; src != NULL; src = src->ai_next) {
		if (src->ai_family != pai->ai_family)
			continue;

		cur->ai_next = copy_ai(src);
		if (!cur->ai_next) {
			error = EAI_MEMORY;
			goto fail;
		}

		cur->ai_next->ai_socktype = pai->ai_socktype;
		cur->ai_next->ai_protocol = pai->ai_protocol;
		cur = cur->ai_next;
	}

	*res = sentinel.ai_next;
	return 0;

fail:
	freeaddrinfo(sentinel.ai_next);
	return error;
}

/*
 * hostname == NULL.
 * passive socket -> anyaddr (0.0.0.0 or ::)
 * non-passive socket -> localhost (127.0.0.1 or ::1)
 */
static int
explore_null(const struct addrinfo *pai, const char *servname,
    struct addrinfo **res)
{
	int s;
	const struct afd *afd;
	struct addrinfo *ai;
	int error;

	*res = NULL;
	ai = NULL;

	/*
	 * filter out AFs that are not supported by the kernel
	 * XXX errno?
	 */
	s = _socket(pai->ai_family, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno != EMFILE)
			return 0;
	} else
		_close(s);

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	if (pai->ai_flags & AI_PASSIVE) {
		GET_AI(ai, afd, afd->a_addrany);
		GET_PORT(ai, servname);
	} else {
		GET_AI(ai, afd, afd->a_loopback);
		GET_PORT(ai, servname);
	}

	*res = ai;
	return 0;

free:
	if (ai != NULL)
		freeaddrinfo(ai);
	return error;
}

/*
 * numeric hostname
 */
static int
explore_numeric(const struct addrinfo *pai, const char *hostname,
    const char *servname, struct addrinfo **res, const char *canonname)
{
	const struct afd *afd;
	struct addrinfo *ai;
	int error;
	char pton[PTON_MAX];

	*res = NULL;
	ai = NULL;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	switch (afd->a_af) {
	case AF_INET:
		/*
		 * RFC3493 requires getaddrinfo() to accept AF_INET formats
		 * that are accepted by inet_addr() and its family.  The
		 * accepted forms includes the "classful" one, which inet_pton
		 * does not accept.  So we need to separate the case for
		 * AF_INET.
		 */
		if (inet_aton(hostname, (struct in_addr *)pton) != 1)
			return 0;
		break;
	default:
		if (inet_pton(afd->a_af, hostname, pton) != 1)
			return 0;
		break;
	}

	if (pai->ai_family == afd->a_af) {
		GET_AI(ai, afd, pton);
		GET_PORT(ai, servname);
		if ((pai->ai_flags & AI_CANONNAME)) {
			/*
			 * Set the numeric address itself as the canonical
			 * name, based on a clarification in RFC3493.
			 */
			GET_CANONNAME(ai, canonname);
		}
	} else {
		/*
		 * XXX: This should not happen since we already matched the AF
		 * by find_afd.
		 */
		ERR(EAI_FAMILY);
	}

	*res = ai;
	return 0;

free:
bad:
	if (ai != NULL)
		freeaddrinfo(ai);
	return error;
}

/*
 * numeric hostname with scope
 */
static int
explore_numeric_scope(const struct addrinfo *pai, const char *hostname,
    const char *servname, struct addrinfo **res)
{
#if !defined(SCOPE_DELIMITER) || !defined(INET6)
	return explore_numeric(pai, hostname, servname, res, hostname);
#else
	const struct afd *afd;
	struct addrinfo *cur;
	int error;
	char *cp, *hostname2 = NULL, *scope, *addr;
	struct sockaddr_in6 *sin6;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	if (!afd->a_scoped)
		return explore_numeric(pai, hostname, servname, res, hostname);

	cp = strchr(hostname, SCOPE_DELIMITER);
	if (cp == NULL)
		return explore_numeric(pai, hostname, servname, res, hostname);

	/*
	 * Handle special case of <scoped_address><delimiter><scope id>
	 */
	hostname2 = strdup(hostname);
	if (hostname2 == NULL)
		return EAI_MEMORY;
	/* terminate at the delimiter */
	hostname2[cp - hostname] = '\0';
	addr = hostname2;
	scope = cp + 1;

	error = explore_numeric(pai, addr, servname, res, hostname);
	if (error == 0) {
		u_int32_t scopeid;

		for (cur = *res; cur; cur = cur->ai_next) {
			if (cur->ai_family != AF_INET6)
				continue;
			sin6 = (struct sockaddr_in6 *)(void *)cur->ai_addr;
			if (ip6_str2scopeid(scope, sin6, &scopeid) == -1) {
				free(hostname2);
				freeaddrinfo(*res);
				*res = NULL;
				return(EAI_NONAME); /* XXX: is return OK? */
			}
			sin6->sin6_scope_id = scopeid;
		}
	}

	free(hostname2);

	if (error && *res) {
		freeaddrinfo(*res);
		*res = NULL;
	}
	return error;
#endif
}

static int
get_canonname(const struct addrinfo *pai, struct addrinfo *ai, const char *str)
{
	if ((pai->ai_flags & AI_CANONNAME) != 0) {
		ai->ai_canonname = strdup(str);
		if (ai->ai_canonname == NULL)
			return EAI_MEMORY;
	}
	return 0;
}

static struct addrinfo *
get_ai(const struct addrinfo *pai, const struct afd *afd, const char *addr)
{
	char *p;
	struct addrinfo *ai;
#ifdef FAITH
	struct in6_addr faith_prefix;
	char *fp_str;
	int translate = 0;
#endif

#ifdef FAITH
	/*
	 * Transfrom an IPv4 addr into a special IPv6 addr format for
	 * IPv6->IPv4 translation gateway. (only TCP is supported now)
	 *
	 * +-----------------------------------+------------+
	 * | faith prefix part (12 bytes)      | embedded   |
	 * |                                   | IPv4 addr part (4 bytes)
	 * +-----------------------------------+------------+
	 *
	 * faith prefix part is specified as ascii IPv6 addr format
	 * in environmental variable GAI.
	 * For FAITH to work correctly, routing to faith prefix must be
	 * setup toward a machine where a FAITH daemon operates.
	 * Also, the machine must enable some mechanizm
	 * (e.g. faith interface hack) to divert those packet with
	 * faith prefixed destination addr to user-land FAITH daemon.
	 */
	fp_str = getenv("GAI");
	if (fp_str && inet_pton(AF_INET6, fp_str, &faith_prefix) == 1 &&
	    afd->a_af == AF_INET && pai->ai_socktype == SOCK_STREAM) {
		u_int32_t v4a;
		u_int8_t v4a_top;

		memcpy(&v4a, addr, sizeof v4a);
		v4a_top = v4a >> IN_CLASSA_NSHIFT;
		if (!IN_MULTICAST(v4a) && !IN_EXPERIMENTAL(v4a) &&
		    v4a_top != 0 && v4a != IN_LOOPBACKNET) {
			afd = &afdl[N_INET6];
			memcpy(&faith_prefix.s6_addr[12], addr,
			       sizeof(struct in_addr));
			translate = 1;
		}
	}
#endif

	ai = (struct addrinfo *)malloc(sizeof(struct addrinfo)
		+ (afd->a_socklen));
	if (ai == NULL)
		return NULL;

	memcpy(ai, pai, sizeof(struct addrinfo));
	ai->ai_addr = (struct sockaddr *)(void *)(ai + 1);
	memset(ai->ai_addr, 0, (size_t)afd->a_socklen);
	ai->ai_addr->sa_len = afd->a_socklen;
	ai->ai_addrlen = afd->a_socklen;
	ai->ai_addr->sa_family = ai->ai_family = afd->a_af;
	p = (char *)(void *)(ai->ai_addr);
#ifdef FAITH
	if (translate == 1)
		memcpy(p + afd->a_off, &faith_prefix, (size_t)afd->a_addrlen);
	else
#endif
	memcpy(p + afd->a_off, addr, (size_t)afd->a_addrlen);
	return ai;
}

/* XXX need to malloc() the same way we do from other functions! */
static struct addrinfo *
copy_ai(const struct addrinfo *pai)
{
	struct addrinfo *ai;
	size_t l;

	l = sizeof(*ai) + pai->ai_addrlen;
	if ((ai = (struct addrinfo *)malloc(l)) == NULL)
		return NULL;
	memset(ai, 0, l);
	memcpy(ai, pai, sizeof(*ai));
	ai->ai_addr = (struct sockaddr *)(void *)(ai + 1);
	memcpy(ai->ai_addr, pai->ai_addr, pai->ai_addrlen);

	if (pai->ai_canonname) {
		l = strlen(pai->ai_canonname) + 1;
		if ((ai->ai_canonname = malloc(l)) == NULL) {
			free(ai);
			return NULL;
		}
		strlcpy(ai->ai_canonname, pai->ai_canonname, l);
	} else {
		/* just to make sure */
		ai->ai_canonname = NULL;
	}

	ai->ai_next = NULL;

	return ai;
}

static int
get_portmatch(const struct addrinfo *ai, const char *servname)
{

	/* get_port does not touch first argument when matchonly == 1. */
	/* LINTED const cast */
	return get_port((struct addrinfo *)ai, servname, 1);
}

static int
get_port(struct addrinfo *ai, const char *servname, int matchonly)
{
	const char *proto;
	struct servent *sp;
	int port, error;
	int allownumeric;

	if (servname == NULL)
		return 0;
	switch (ai->ai_family) {
	case AF_INET:
#ifdef AF_INET6
	case AF_INET6:
#endif
		break;
	default:
		return 0;
	}

	switch (ai->ai_socktype) {
	case SOCK_RAW:
		return EAI_SERVICE;
	case SOCK_DGRAM:
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		allownumeric = 1;
		break;
	case ANY:
		switch (ai->ai_family) {
		case AF_INET:
#ifdef AF_INET6
		case AF_INET6:
#endif
			allownumeric = 1;
			break;
		default:
			allownumeric = 0;
			break;
		}
		break;
	default:
		return EAI_SOCKTYPE;
	}

	error = str2number(servname, &port);
	if (error == 0) {
		if (!allownumeric)
			return EAI_SERVICE;
		if (port < 0 || port > 65535)
			return EAI_SERVICE;
		port = htons(port);
	} else {
		if (ai->ai_flags & AI_NUMERICSERV)
			return EAI_NONAME;

		switch (ai->ai_protocol) {
		case IPPROTO_UDP:
			proto = "udp";
			break;
		case IPPROTO_TCP:
			proto = "tcp";
			break;
		case IPPROTO_SCTP:
			proto = "sctp";
			break;
		default:
			proto = NULL;
			break;
		}

		if ((sp = getservbyname(servname, proto)) == NULL)
			return EAI_SERVICE;
		port = sp->s_port;
	}

	if (!matchonly) {
		switch (ai->ai_family) {
		case AF_INET:
			((struct sockaddr_in *)(void *)
			    ai->ai_addr)->sin_port = port;
			break;
#ifdef INET6
		case AF_INET6:
			((struct sockaddr_in6 *)(void *)
			    ai->ai_addr)->sin6_port = port;
			break;
#endif
		}
	}

	return 0;
}

static const struct afd *
find_afd(int af)
{
	const struct afd *afd;

	if (af == PF_UNSPEC)
		return NULL;
	for (afd = afdl; afd->a_af; afd++) {
		if (afd->a_af == af)
			return afd;
	}
	return NULL;
}

/*
 * post-2553: AI_ADDRCONFIG check.  if we use getipnodeby* as backend, backend
 * will take care of it.
 * the semantics of AI_ADDRCONFIG is not defined well.  we are not sure
 * if the code is right or not.
 *
 * XXX PF_UNSPEC -> PF_INET6 + PF_INET mapping needs to be in sync with
 * _dns_getaddrinfo.
 */
static int
addrconfig(struct addrinfo *pai)
{
	int s, af;

	/*
	 * TODO:
	 * Note that implementation dependent test for address
	 * configuration should be done everytime called
	 * (or apropriate interval),
	 * because addresses will be dynamically assigned or deleted.
	 */
	af = pai->ai_family;
	if (af == AF_UNSPEC) {
		if ((s = _socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			af = AF_INET;
		else {
			_close(s);
			if ((s = _socket(AF_INET, SOCK_DGRAM, 0)) < 0)
				af = AF_INET6;
			else
				_close(s);
		}
	}
	if (af != AF_UNSPEC) {
		if ((s = _socket(af, SOCK_DGRAM, 0)) < 0)
			return 0;
		_close(s);
	}
	pai->ai_family = af;
	return 1;
}

#ifdef INET6
/* convert a string to a scope identifier. XXX: IPv6 specific */
static int
ip6_str2scopeid(char *scope, struct sockaddr_in6 *sin6, u_int32_t *scopeid)
{
	u_long lscopeid;
	struct in6_addr *a6;
	char *ep;

	a6 = &sin6->sin6_addr;

	/* empty scopeid portion is invalid */
	if (*scope == '\0')
		return -1;

	if (IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6) ||
	    IN6_IS_ADDR_MC_NODELOCAL(a6)) {
		/*
		 * We currently assume a one-to-one mapping between links
		 * and interfaces, so we simply use interface indices for
		 * like-local scopes.
		 */
		*scopeid = if_nametoindex(scope);
		if (*scopeid == 0)
			goto trynumeric;
		return 0;
	}

	/* still unclear about literal, allow numeric only - placeholder */
	if (IN6_IS_ADDR_SITELOCAL(a6) || IN6_IS_ADDR_MC_SITELOCAL(a6))
		goto trynumeric;
	if (IN6_IS_ADDR_MC_ORGLOCAL(a6))
		goto trynumeric;
	else
		goto trynumeric;	/* global */

	/* try to convert to a numeric id as a last resort */
  trynumeric:
	errno = 0;
	lscopeid = strtoul(scope, &ep, 10);
	*scopeid = (u_int32_t)(lscopeid & 0xffffffffUL);
	if (errno == 0 && ep && *ep == '\0' && *scopeid == lscopeid)
		return 0;
	else
		return -1;
}
#endif


#ifdef NS_CACHING
static int
addrinfo_id_func(char *buffer, size_t *buffer_size, va_list ap,
    void *cache_mdata)
{
	res_state statp;
	u_long res_options;

	const int op_id = 0;	/* identifies the getaddrinfo for the cache */
	char *hostname;
	struct addrinfo *hints;

	char *p;
	int ai_flags, ai_family, ai_socktype, ai_protocol;
	size_t desired_size, size;

	statp = __res_state();
	res_options = statp->options & (RES_RECURSE | RES_DEFNAMES |
	    RES_DNSRCH | RES_NOALIASES | RES_USE_INET6);

	hostname = va_arg(ap, char *);
	hints = va_arg(ap, struct addrinfo *);

	desired_size = sizeof(res_options) + sizeof(int) + sizeof(int) * 4;
	if (hostname != NULL) {
		size = strlen(hostname);
		desired_size += size + 1;
	} else
		size = 0;

	if (desired_size > *buffer_size) {
		*buffer_size = desired_size;
		return (NS_RETURN);
	}

	if (hints == NULL)
		ai_flags = ai_family = ai_socktype = ai_protocol = 0;
	else {
		ai_flags = hints->ai_flags;
		ai_family = hints->ai_family;
		ai_socktype = hints->ai_socktype;
		ai_protocol = hints->ai_protocol;
	}

	p = buffer;
	memcpy(p, &res_options, sizeof(res_options));
	p += sizeof(res_options);

	memcpy(p, &op_id, sizeof(int));
	p += sizeof(int);

	memcpy(p, &ai_flags, sizeof(int));
	p += sizeof(int);

	memcpy(p, &ai_family, sizeof(int));
	p += sizeof(int);

	memcpy(p, &ai_socktype, sizeof(int));
	p += sizeof(int);

	memcpy(p, &ai_protocol, sizeof(int));
	p += sizeof(int);

	if (hostname != NULL)
		memcpy(p, hostname, size);

	*buffer_size = desired_size;
	return (NS_SUCCESS);
}

static int
addrinfo_marshal_func(char *buffer, size_t *buffer_size, void *retval,
    va_list ap, void *cache_mdata)
{
	struct addrinfo	*ai, *cai;
	char *p;
	size_t desired_size, size, ai_size;

	ai = *((struct addrinfo **)retval);

	desired_size = sizeof(size_t);
	ai_size = 0;
	for (cai = ai; cai != NULL; cai = cai->ai_next) {
		desired_size += sizeof(struct addrinfo) + cai->ai_addrlen;
		if (cai->ai_canonname != NULL)
			desired_size += sizeof(size_t) +
			    strlen(cai->ai_canonname);
		++ai_size;
	}

	if (desired_size > *buffer_size) {
		/* this assignment is here for future use */
		errno = ERANGE;
		*buffer_size = desired_size;
		return (NS_RETURN);
	}

	memset(buffer, 0, desired_size);
	p = buffer;

	memcpy(p, &ai_size, sizeof(size_t));
	p += sizeof(size_t);
	for (cai = ai; cai != NULL; cai = cai->ai_next) {
		memcpy(p, cai, sizeof(struct addrinfo));
		p += sizeof(struct addrinfo);

		memcpy(p, cai->ai_addr, cai->ai_addrlen);
		p += cai->ai_addrlen;

		if (cai->ai_canonname != NULL) {
			size = strlen(cai->ai_canonname);
			memcpy(p, &size, sizeof(size_t));
			p += sizeof(size_t);

			memcpy(p, cai->ai_canonname, size);
			p += size;
		}
	}

	return (NS_SUCCESS);
}

static int
addrinfo_unmarshal_func(char *buffer, size_t buffer_size, void *retval,
    va_list ap, void *cache_mdata)
{
	struct addrinfo	new_ai, *result, *sentinel, *lasts;

	char *p;
	size_t ai_size, ai_i, size;

	p = buffer;
	memcpy(&ai_size, p, sizeof(size_t));
	p += sizeof(size_t);

	result = NULL;
	lasts = NULL;
	for (ai_i = 0; ai_i < ai_size; ++ai_i) {
		memcpy(&new_ai, p, sizeof(struct addrinfo));
		p += sizeof(struct addrinfo);
		size = new_ai.ai_addrlen + sizeof(struct addrinfo) +
			_ALIGNBYTES;

		sentinel = (struct addrinfo *)malloc(size);
		memset(sentinel, 0, size);

		memcpy(sentinel, &new_ai, sizeof(struct addrinfo));
		sentinel->ai_addr = (struct sockaddr *)_ALIGN((char *)sentinel +
		    sizeof(struct addrinfo));

		memcpy(sentinel->ai_addr, p, new_ai.ai_addrlen);
		p += new_ai.ai_addrlen;

		if (new_ai.ai_canonname != NULL) {
			memcpy(&size, p, sizeof(size_t));
			p += sizeof(size_t);

			sentinel->ai_canonname = (char *)malloc(size + 1);
			memset(sentinel->ai_canonname, 0, size + 1);

			memcpy(sentinel->ai_canonname, p, size);
			p += size;
		}

		if (result == NULL) {
			result = sentinel;
			lasts = sentinel;
		} else {
			lasts->ai_next = sentinel;
			lasts = sentinel;
		}
	}

	*((struct addrinfo **)retval) = result;
	return (NS_SUCCESS);
}
#endif /* NS_CACHING */

/*
 * FQDN hostname, DNS lookup
 */
static int
explore_fqdn(const struct addrinfo *pai, const char *hostname,
    const char *servname, struct addrinfo **res)
{
	struct addrinfo *result;
	struct addrinfo *cur;
	int error = 0;

#ifdef NS_CACHING
	static const nss_cache_info cache_info =
	NS_COMMON_CACHE_INFO_INITIALIZER(
		hosts, NULL, addrinfo_id_func, addrinfo_marshal_func,
		addrinfo_unmarshal_func);
#endif
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getaddrinfo, NULL)
		{ NSSRC_DNS, _dns_getaddrinfo, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_getaddrinfo, NULL)
#ifdef NS_CACHING
		NS_CACHE_CB(&cache_info)
#endif
		{ 0 }
	};

	result = NULL;

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

	switch (_nsdispatch(&result, dtab, NSDB_HOSTS, "getaddrinfo",
			default_dns_files, hostname, pai)) {
	case NS_TRYAGAIN:
		error = EAI_AGAIN;
		goto free;
	case NS_UNAVAIL:
		error = EAI_FAIL;
		goto free;
	case NS_NOTFOUND:
		error = EAI_NONAME;
		goto free;
	case NS_SUCCESS:
		error = 0;
		for (cur = result; cur; cur = cur->ai_next) {
			GET_PORT(cur, servname);
			/* canonname should be filled already */
		}
		break;
	}

	*res = result;

	return 0;

free:
	if (result)
		freeaddrinfo(result);
	return error;
}

#ifdef DEBUG
static const char AskedForGot[] =
	"gethostby*.getanswer: asked for \"%s\", got \"%s\"";
#endif

static struct addrinfo *
getanswer(const querybuf *answer, int anslen, const char *qname, int qtype,
    const struct addrinfo *pai, res_state res)
{
	struct addrinfo sentinel, *cur;
	struct addrinfo ai;
	const struct afd *afd;
	char *canonname;
	const HEADER *hp;
	const u_char *cp;
	int n;
	const u_char *eom;
	char *bp, *ep;
	int type, class, ancount, qdcount;
	int haveanswer, had_error;
	char tbuf[MAXDNAME];
	int (*name_ok)(const char *);
	char hostbuf[8*1024];

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	canonname = NULL;
	eom = answer->buf + anslen;
	switch (qtype) {
	case T_A:
	case T_AAAA:
	case T_ANY:	/*use T_ANY only for T_A/T_AAAA lookup*/
		name_ok = res_hnok;
		break;
	default:
		return (NULL);	/* XXX should be abort(); */
	}
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = hostbuf;
	ep = hostbuf + sizeof hostbuf;
	cp = answer->buf + HFIXEDSZ;
	if (qdcount != 1) {
		RES_SET_H_ERRNO(res, NO_RECOVERY);
		return (NULL);
	}
	n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
	if ((n < 0) || !(*name_ok)(bp)) {
		RES_SET_H_ERRNO(res, NO_RECOVERY);
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (qtype == T_A || qtype == T_AAAA || qtype == T_ANY) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = strlen(bp) + 1;		/* for the \0 */
		if (n >= MAXHOSTNAMELEN) {
			RES_SET_H_ERRNO(res, NO_RECOVERY);
			return (NULL);
		}
		canonname = bp;
		bp += n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = canonname;
	}
	haveanswer = 0;
	had_error = 0;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
		if ((n < 0) || !(*name_ok)(bp)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		if (class != C_IN) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if ((qtype == T_A || qtype == T_AAAA || qtype == T_ANY) &&
		    type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if ((n < 0) || !(*name_ok)(tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strlcpy(bp, tbuf, ep - bp);
			canonname = bp;
			bp += n;
			continue;
		}
		if (qtype == T_ANY) {
			if (!(type == T_A || type == T_AAAA)) {
				cp += n;
				continue;
			}
		} else if (type != qtype) {
#ifdef DEBUG
			if (type != T_KEY && type != T_SIG &&
			    type != ns_t_dname)
				syslog(LOG_NOTICE|LOG_AUTH,
	       "gethostby*.getanswer: asked for \"%s %s %s\", got type \"%s\"",
				       qname, p_class(C_IN), p_type(qtype),
				       p_type(type));
#endif
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		switch (type) {
		case T_A:
		case T_AAAA:
			if (strcasecmp(canonname, bp) != 0) {
#ifdef DEBUG
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, canonname, bp);
#endif
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (type == T_A && n != INADDRSZ) {
				cp += n;
				continue;
			}
			if (type == T_AAAA && n != IN6ADDRSZ) {
				cp += n;
				continue;
			}
#ifdef FILTER_V4MAPPED
			if (type == T_AAAA) {
				struct in6_addr in6;
				memcpy(&in6, cp, sizeof(in6));
				if (IN6_IS_ADDR_V4MAPPED(&in6)) {
					cp += n;
					continue;
				}
			}
#endif
			if (!haveanswer) {
				int nn;

				canonname = bp;
				nn = strlen(bp) + 1;	/* for the \0 */
				bp += nn;
			}

			/* don't overwrite pai */
			ai = *pai;
			ai.ai_family = (type == T_A) ? AF_INET : AF_INET6;
			afd = find_afd(ai.ai_family);
			if (afd == NULL) {
				cp += n;
				continue;
			}
			cur->ai_next = get_ai(&ai, afd, (const char *)cp);
			if (cur->ai_next == NULL)
				had_error++;
			while (cur && cur->ai_next)
				cur = cur->ai_next;
			cp += n;
			break;
		default:
			abort();
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
#if defined(RESOLVSORT)
		/*
		 * We support only IPv4 address for backward
		 * compatibility against gethostbyname(3).
		 */
		if (res->nsort && qtype == T_A) {
			if (addr4sort(&sentinel, res) < 0) {
				freeaddrinfo(sentinel.ai_next);
				RES_SET_H_ERRNO(res, NO_RECOVERY);
				return NULL;
			}
		}
#endif /*RESOLVSORT*/
		if (!canonname)
			(void)get_canonname(pai, sentinel.ai_next, qname);
		else
			(void)get_canonname(pai, sentinel.ai_next, canonname);
		RES_SET_H_ERRNO(res, NETDB_SUCCESS);
		return sentinel.ai_next;
	}

	RES_SET_H_ERRNO(res, NO_RECOVERY);
	return NULL;
}

#ifdef RESOLVSORT
struct addr_ptr {
	struct addrinfo *ai;
	int aval;
};

static int
addr4sort(struct addrinfo *sentinel, res_state res)
{
	struct addrinfo *ai;
	struct addr_ptr *addrs, addr;
	struct sockaddr_in *sin;
	int naddrs, i, j;
	int needsort = 0;

	if (!sentinel)
		return -1;
	naddrs = 0;
	for (ai = sentinel->ai_next; ai; ai = ai->ai_next)
		naddrs++;
	if (naddrs < 2)
		return 0;		/* We don't need sorting. */
	if ((addrs = malloc(sizeof(struct addr_ptr) * naddrs)) == NULL)
		return -1;
	i = 0;
	for (ai = sentinel->ai_next; ai; ai = ai->ai_next) {
		sin = (struct sockaddr_in *)ai->ai_addr;
		for (j = 0; (unsigned)j < res->nsort; j++) {
			if (res->sort_list[j].addr.s_addr ==
			    (sin->sin_addr.s_addr & res->sort_list[j].mask))
				break;
		}
		addrs[i].ai = ai;
		addrs[i].aval = j;
		if (needsort == 0 && i > 0 && j < addrs[i - 1].aval)
			needsort = i;
		i++;
	}
	if (!needsort) {
		free(addrs);
		return 0;
	}

	while (needsort < naddrs) {
		for (j = needsort - 1; j >= 0; j--) {
			if (addrs[j].aval > addrs[j+1].aval) {
				addr = addrs[j];
				addrs[j] = addrs[j + 1];
				addrs[j + 1] = addr;
			} else
				break;
		}
		needsort++;
	}

	ai = sentinel;
	for (i = 0; i < naddrs; ++i) {
		ai->ai_next = addrs[i].ai;
		ai = ai->ai_next;
	}
	ai->ai_next = NULL;
	free(addrs);
	return 0;
}
#endif /*RESOLVSORT*/

/*ARGSUSED*/
static int
_dns_getaddrinfo(void *rv, void *cb_data, va_list ap)
{
	struct addrinfo *ai;
	querybuf *buf, *buf2;
	const char *hostname;
	const struct addrinfo *pai;
	struct addrinfo sentinel, *cur;
	struct res_target q, q2;
	res_state res;

	hostname = va_arg(ap, char *);
	pai = va_arg(ap, const struct addrinfo *);

	memset(&q, 0, sizeof(q));
	memset(&q2, 0, sizeof(q2));
	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	buf = malloc(sizeof(*buf));
	if (!buf) {
		RES_SET_H_ERRNO(res, NETDB_INTERNAL);
		return NS_NOTFOUND;
	}
	buf2 = malloc(sizeof(*buf2));
	if (!buf2) {
		free(buf);
		RES_SET_H_ERRNO(res, NETDB_INTERNAL);
		return NS_NOTFOUND;
	}

	switch (pai->ai_family) {
	case AF_UNSPEC:
		q.name = hostname;
		q.qclass = C_IN;
		q.qtype = T_A;
		q.answer = buf->buf;
		q.anslen = sizeof(buf->buf);
		q.next = &q2;
		q2.name = hostname;
		q2.qclass = C_IN;
		q2.qtype = T_AAAA;
		q2.answer = buf2->buf;
		q2.anslen = sizeof(buf2->buf);
		break;
	case AF_INET:
		q.name = hostname;
		q.qclass = C_IN;
		q.qtype = T_A;
		q.answer = buf->buf;
		q.anslen = sizeof(buf->buf);
		break;
	case AF_INET6:
		q.name = hostname;
		q.qclass = C_IN;
		q.qtype = T_AAAA;
		q.answer = buf->buf;
		q.anslen = sizeof(buf->buf);
		break;
	default:
		free(buf);
		free(buf2);
		return NS_UNAVAIL;
	}

	res = __res_state();
	if ((res->options & RES_INIT) == 0 && res_ninit(res) == -1) {
		RES_SET_H_ERRNO(res, NETDB_INTERNAL);
		free(buf);
		free(buf2);
		return NS_NOTFOUND;
	}

	if (res_searchN(hostname, &q, res) < 0) {
		free(buf);
		free(buf2);
		return NS_NOTFOUND;
	}
	/* prefer IPv6 */
	if (q.next) {
		ai = getanswer(buf2, q2.n, q2.name, q2.qtype, pai, res);
		if (ai) {
			cur->ai_next = ai;
			while (cur && cur->ai_next)
				cur = cur->ai_next;
		}
	}
	ai = getanswer(buf, q.n, q.name, q.qtype, pai, res);
	if (ai)
		cur->ai_next = ai;
	free(buf);
	free(buf2);
	if (sentinel.ai_next == NULL)
		switch (res->res_h_errno) {
		case HOST_NOT_FOUND:
			return NS_NOTFOUND;
		case TRY_AGAIN:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	*((struct addrinfo **)rv) = sentinel.ai_next;
	return NS_SUCCESS;
}

static void
_sethtent(FILE **hostf)
{
	if (!*hostf)
		*hostf = fopen(_PATH_HOSTS, "r");
	else
		rewind(*hostf);
}

static void
_endhtent(FILE **hostf)
{
	if (*hostf) {
		(void) fclose(*hostf);
		*hostf = NULL;
	}
}

static struct addrinfo *
_gethtent(FILE **hostf, const char *name, const struct addrinfo *pai)
{
	char *p;
	char *cp, *tname, *cname;
	struct addrinfo hints, *res0, *res;
	int error;
	const char *addr;
	char hostbuf[8*1024];

	if (!*hostf && !(*hostf = fopen(_PATH_HOSTS, "r")))
		return (NULL);
again:
	if (!(p = fgets(hostbuf, sizeof hostbuf, *hostf)))
		return (NULL);
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp != NULL)
		*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	addr = p;
	cname = NULL;
	/* if this is not something we're looking for, skip it. */
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		tname = cp;
		if (cname == NULL)
			cname = cp;
		if ((cp = strpbrk(cp, " \t")) != NULL)
			*cp++ = '\0';
		if (strcasecmp(name, tname) == 0)
			goto found;
	}
	goto again;

found:
	/* we should not glob socktype/protocol here */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = pai->ai_family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo(addr, "0", &hints, &res0);
	if (error)
		goto again;
#ifdef FILTER_V4MAPPED
	/* XXX should check all items in the chain */
	if (res0->ai_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)res0->ai_addr)->sin6_addr)) {
		freeaddrinfo(res0);
		goto again;
	}
#endif
	for (res = res0; res; res = res->ai_next) {
		/* cover it up */
		res->ai_flags = pai->ai_flags;
		res->ai_socktype = pai->ai_socktype;
		res->ai_protocol = pai->ai_protocol;

		if (pai->ai_flags & AI_CANONNAME) {
			if (get_canonname(pai, res, cname) != 0) {
				freeaddrinfo(res0);
				goto again;
			}
		}
	}
	return res0;
}

/*ARGSUSED*/
static int
_files_getaddrinfo(void *rv, void *cb_data, va_list ap)
{
	const char *name;
	const struct addrinfo *pai;
	struct addrinfo sentinel, *cur;
	struct addrinfo *p;
	FILE *hostf = NULL;

	name = va_arg(ap, char *);
	pai = va_arg(ap, struct addrinfo *);

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	_sethtent(&hostf);
	while ((p = _gethtent(&hostf, name, pai)) != NULL) {
		cur->ai_next = p;
		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}
	_endhtent(&hostf);

	*((struct addrinfo **)rv) = sentinel.ai_next;
	if (sentinel.ai_next == NULL)
		return NS_NOTFOUND;
	return NS_SUCCESS;
}

#ifdef YP
/*ARGSUSED*/
static struct addrinfo *
_yphostent(char *line, const struct addrinfo *pai)
{
	struct addrinfo sentinel, *cur;
	struct addrinfo hints, *res, *res0;
	int error;
	char *p = line;
	const char *addr, *canonname;
	char *nextline;
	char *cp;

	addr = canonname = NULL;

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

nextline:
	/* terminate line */
	cp = strchr(p, '\n');
	if (cp) {
		*cp++ = '\0';
		nextline = cp;
	} else
		nextline = NULL;

	cp = strpbrk(p, " \t");
	if (cp == NULL) {
		if (canonname == NULL)
			return (NULL);
		else
			goto done;
	}
	*cp++ = '\0';

	addr = p;

	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (!canonname)
			canonname = cp;
		if ((cp = strpbrk(cp, " \t")) != NULL)
			*cp++ = '\0';
	}

	hints = *pai;
	hints.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo(addr, NULL, &hints, &res0);
	if (error == 0) {
		for (res = res0; res; res = res->ai_next) {
			/* cover it up */
			res->ai_flags = pai->ai_flags;

			if (pai->ai_flags & AI_CANONNAME)
				(void)get_canonname(pai, res, canonname);
		}
	} else
		res0 = NULL;
	if (res0) {
		cur->ai_next = res0;
		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}

	if (nextline) {
		p = nextline;
		goto nextline;
	}

done:
	return sentinel.ai_next;
}

/*ARGSUSED*/
static int
_yp_getaddrinfo(void *rv, void *cb_data, va_list ap)
{
	struct addrinfo sentinel, *cur;
	struct addrinfo *ai = NULL;
	char *ypbuf;
	int ypbuflen, r;
	const char *name;
	const struct addrinfo *pai;
	char *ypdomain;

	if (_yp_check(&ypdomain) == 0)
		return NS_UNAVAIL;

	name = va_arg(ap, char *);
	pai = va_arg(ap, const struct addrinfo *);

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	/* hosts.byname is only for IPv4 (Solaris8) */
	if (pai->ai_family == PF_UNSPEC || pai->ai_family == PF_INET) {
		r = yp_match(ypdomain, "hosts.byname", name,
			(int)strlen(name), &ypbuf, &ypbuflen);
		if (r == 0) {
			struct addrinfo ai4;

			ai4 = *pai;
			ai4.ai_family = AF_INET;
			ai = _yphostent(ypbuf, &ai4);
			if (ai) {
				cur->ai_next = ai;
				while (cur && cur->ai_next)
					cur = cur->ai_next;
			}
			free(ypbuf);
		}
	}

	/* ipnodes.byname can hold both IPv4/v6 */
	r = yp_match(ypdomain, "ipnodes.byname", name,
		(int)strlen(name), &ypbuf, &ypbuflen);
	if (r == 0) {
		ai = _yphostent(ypbuf, pai);
		if (ai)
			cur->ai_next = ai;
		free(ypbuf);
	}

	if (sentinel.ai_next == NULL) {
		RES_SET_H_ERRNO(__res_state(), HOST_NOT_FOUND);
		return NS_NOTFOUND;
	}
	*((struct addrinfo **)rv) = sentinel.ai_next;
	return NS_SUCCESS;
}
#endif

/* resolver logic */

/*
 * Formulate a normal query, send, and await answer.
 * Returned answer is placed in supplied buffer "answer".
 * Perform preliminary check of answer, returning success only
 * if no error is indicated and the answer count is nonzero.
 * Return the size of the response on success, -1 on error.
 * Error number is left in h_errno.
 *
 * Caller must parse answer and determine whether it answers the question.
 */
static int
res_queryN(const char *name, struct res_target *target, res_state res)
{
	u_char *buf;
	HEADER *hp;
	int n;
	u_int oflags;
	struct res_target *t;
	int rcode;
	int ancount;

	rcode = NOERROR;
	ancount = 0;

	buf = malloc(MAXPACKET);
	if (!buf) {
		RES_SET_H_ERRNO(res, NETDB_INTERNAL);
		return -1;
	}

	for (t = target; t; t = t->next) {
		int class, type;
		u_char *answer;
		int anslen;

		hp = (HEADER *)(void *)t->answer;

		/* make it easier... */
		class = t->qclass;
		type = t->qtype;
		answer = t->answer;
		anslen = t->anslen;

		oflags = res->_flags;

again:
		hp->rcode = NOERROR;	/* default */

#ifdef DEBUG
		if (res->options & RES_DEBUG)
			printf(";; res_query(%s, %d, %d)\n", name, class, type);
#endif

		n = res_nmkquery(res, QUERY, name, class, type, NULL, 0, NULL,
		    buf, MAXPACKET);
		if (n > 0 && (res->_flags & RES_F_EDNS0ERR) == 0 &&
		    (res->options & (RES_USE_EDNS0|RES_USE_DNSSEC)) != 0U)
			n = res_nopt(res, n, buf, MAXPACKET, anslen);
		if (n <= 0) {
#ifdef DEBUG
			if (res->options & RES_DEBUG)
				printf(";; res_query: mkquery failed\n");
#endif
			free(buf);
			RES_SET_H_ERRNO(res, NO_RECOVERY);
			return (n);
		}
		n = res_nsend(res, buf, n, answer, anslen);
		if (n < 0) {
			/*
			 * if the query choked with EDNS0, retry
			 * without EDNS0
			 */
			if ((res->options & (RES_USE_EDNS0|RES_USE_DNSSEC))
			    != 0U &&
			    ((oflags ^ res->_flags) & RES_F_EDNS0ERR) != 0) {
				res->_flags |= RES_F_EDNS0ERR;
				if (res->options & RES_DEBUG)
					printf(";; res_nquery: retry without EDNS0\n");
				goto again;
			}
			rcode = hp->rcode;	/* record most recent error */
#ifdef DEBUG
			if (res->options & RES_DEBUG)
				printf(";; res_query: send error\n");
#endif
			continue;
		}

		if (n > anslen)
			hp->rcode = FORMERR; /* XXX not very informative */
		if (hp->rcode != NOERROR || ntohs(hp->ancount) == 0) {
			rcode = hp->rcode;	/* record most recent error */
#ifdef DEBUG
			if (res->options & RES_DEBUG)
				printf(";; rcode = %u, ancount=%u\n", hp->rcode,
				    ntohs(hp->ancount));
#endif
			continue;
		}

		ancount += ntohs(hp->ancount);

		t->n = n;
	}

	free(buf);

	if (ancount == 0) {
		switch (rcode) {
		case NXDOMAIN:
			RES_SET_H_ERRNO(res, HOST_NOT_FOUND);
			break;
		case SERVFAIL:
			RES_SET_H_ERRNO(res, TRY_AGAIN);
			break;
		case NOERROR:
			RES_SET_H_ERRNO(res, NO_DATA);
			break;
		case FORMERR:
		case NOTIMP:
		case REFUSED:
		default:
			RES_SET_H_ERRNO(res, NO_RECOVERY);
			break;
		}
		return (-1);
	}
	return (ancount);
}

/*
 * Formulate a normal query, send, and retrieve answer in supplied buffer.
 * Return the size of the response on success, -1 on error.
 * If enabled, implement search rules until answer or unrecoverable failure
 * is detected.  Error code, if any, is left in h_errno.
 */
static int
res_searchN(const char *name, struct res_target *target, res_state res)
{
	const char *cp, * const *domain;
	HEADER *hp = (HEADER *)(void *)target->answer;	/*XXX*/
	u_int dots;
	int trailing_dot, ret, saved_herrno;
	int got_nodata = 0, got_servfail = 0, root_on_list = 0;
	int tried_as_is = 0;
	int searched = 0;
	char abuf[MAXDNAME];

	errno = 0;
	RES_SET_H_ERRNO(res, HOST_NOT_FOUND); /* default, if we never query */
	dots = 0;
	for (cp = name; *cp; cp++)
		dots += (*cp == '.');
	trailing_dot = 0;
	if (cp > name && *--cp == '.')
		trailing_dot++;

	/*
	 * if there aren't any dots, it could be a user-level alias
	 */
	if (!dots &&
	    (cp = res_hostalias(res, name, abuf, sizeof(abuf))) != NULL)
		return (res_queryN(cp, target, res));

	/*
	 * If there are enough dots in the name, let's just give it a
	 * try 'as is'. The threshold can be set with the "ndots" option.
	 * Also, query 'as is', if there is a trailing dot in the name.
	 */
	saved_herrno = -1;
	if (dots >= res->ndots || trailing_dot) {
		ret = res_querydomainN(name, NULL, target, res);
		if (ret > 0 || trailing_dot)
			return (ret);
		if (errno == ECONNREFUSED) {
			RES_SET_H_ERRNO(res, TRY_AGAIN);
			return (-1);
		}
		switch (res->res_h_errno) {
		case NO_DATA:
		case HOST_NOT_FOUND:
			break;
		case TRY_AGAIN:
			if (hp->rcode == SERVFAIL)
				break;
			/* FALLTHROUGH */
		default:
			return (-1);
		}
		saved_herrno = res->res_h_errno;
		tried_as_is++;
	}

	/*
	 * We do at least one level of search if
	 *	- there is no dot and RES_DEFNAME is set, or
	 *	- there is at least one dot, there is no trailing dot,
	 *	  and RES_DNSRCH is set.
	 */
	if ((!dots && (res->options & RES_DEFNAMES)) ||
	    (dots && !trailing_dot && (res->options & RES_DNSRCH))) {
		int done = 0;

		for (domain = (const char * const *)res->dnsrch;
		   *domain && !done;
		   domain++) {
			searched = 1;

			if (domain[0][0] == '\0' ||
			    (domain[0][0] == '.' && domain[0][1] == '\0'))
				root_on_list++;

			if (root_on_list && tried_as_is)
				continue;

			ret = res_querydomainN(name, *domain, target, res);
			if (ret > 0)
				return (ret);

			/*
			 * If no server present, give up.
			 * If name isn't found in this domain,
			 * keep trying higher domains in the search list
			 * (if that's enabled).
			 * On a NO_DATA error, keep trying, otherwise
			 * a wildcard entry of another type could keep us
			 * from finding this entry higher in the domain.
			 * If we get some other error (negative answer or
			 * server failure), then stop searching up,
			 * but try the input name below in case it's
			 * fully-qualified.
			 */
			if (errno == ECONNREFUSED) {
				RES_SET_H_ERRNO(res, TRY_AGAIN);
				return (-1);
			}

			switch (res->res_h_errno) {
			case NO_DATA:
				got_nodata++;
				/* FALLTHROUGH */
			case HOST_NOT_FOUND:
				/* keep trying */
				break;
			case TRY_AGAIN:
				got_servfail++;
				if (hp->rcode == SERVFAIL) {
					/* try next search element, if any */
					break;
				}
				/* FALLTHROUGH */
			default:
				/* anything else implies that we're done */
				done++;
			}
			/*
			 * if we got here for some reason other than DNSRCH,
			 * we only wanted one iteration of the loop, so stop.
			 */
			if (!(res->options & RES_DNSRCH))
			        done++;
		}
	}

	switch (res->res_h_errno) {
	case NO_DATA:
	case HOST_NOT_FOUND:
		break;
	case TRY_AGAIN:
		if (hp->rcode == SERVFAIL)
			break;
		/* FALLTHROUGH */
	default:
		goto giveup;
	}

	/*
	 * If the query has not already been tried as is then try it
	 * unless RES_NOTLDQUERY is set and there were no dots.
	 */
	if ((dots || !searched || !(res->options & RES_NOTLDQUERY)) &&
	    !(tried_as_is || root_on_list)) {
		ret = res_querydomainN(name, NULL, target, res);
		if (ret > 0)
			return (ret);
	}

	/*
	 * if we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's h_errno
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless h_errno, that being the one from
	 * the last DNSRCH we did.
	 */
giveup:
	if (saved_herrno != -1)
		RES_SET_H_ERRNO(res, saved_herrno);
	else if (got_nodata)
		RES_SET_H_ERRNO(res, NO_DATA);
	else if (got_servfail)
		RES_SET_H_ERRNO(res, TRY_AGAIN);
	return (-1);
}

/*
 * Perform a call on res_query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
static int
res_querydomainN(const char *name, const char *domain,
    struct res_target *target, res_state res)
{
	char nbuf[MAXDNAME];
	const char *longname = nbuf;
	size_t n, d;

#ifdef DEBUG
	if (res->options & RES_DEBUG)
		printf(";; res_querydomain(%s, %s)\n",
			name, domain?domain:"<Nil>");
#endif
	if (domain == NULL) {
		/*
		 * Check for trailing '.';
		 * copy without '.' if present.
		 */
		n = strlen(name);
		if (n >= MAXDNAME) {
			RES_SET_H_ERRNO(res, NO_RECOVERY);
			return (-1);
		}
		if (n > 0 && name[--n] == '.') {
			strncpy(nbuf, name, n);
			nbuf[n] = '\0';
		} else
			longname = name;
	} else {
		n = strlen(name);
		d = strlen(domain);
		if (n + d + 1 >= MAXDNAME) {
			RES_SET_H_ERRNO(res, NO_RECOVERY);
			return (-1);
		}
		snprintf(nbuf, sizeof(nbuf), "%s.%s", name, domain);
	}
	return (res_queryN(longname, target, res));
}
