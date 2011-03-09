/* $MidnightBSD: src/apache/lib/libnss_mdns/netbsd.c,v 1.1 2011/03/08 22:13:48 laffer1 Exp $ */
/* $NetBSD: netbsd.c,v 1.1.1.1 2006/07/10 17:35:30 drochner Exp $ */

#include <sys/param.h>
#include <nsswitch.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "netbsd.h"
typedef enum nss_status NSS_STATUS;

extern NSS_STATUS _nss_mdns_gethostbyname2_r(const char *,
	int, struct hostent *, char *, size_t, int *, int *);
static int netbsd_gethostbyname(void *, void *, va_list);
static int netbsd_gethostbyname2_r(void *, void *, va_list);
static int netbsd_getaddrinfo(void *, void *, va_list);
extern NSS_STATUS _nss_mdns_gethostbyaddr_r(const void *, socklen_t,
	int, struct hostent *, char *, size_t, int *, int *);
static int netbsd_gethostbyaddr(void *, void *, va_list);
static int netbsd_gethostbyaddr_r(void *rv, void *cb_data, va_list ap);

static int nss2netbsderr[] = {
	NS_SUCCESS, NS_NOTFOUND, NS_UNAVAIL, NS_TRYAGAIN, NS_RETURN
};

static struct hostent host;
static char hostbuf[8*1024];

static ns_mtab methods[] = {
	{ NSDB_HOSTS, "gethostbyname", netbsd_gethostbyname, 0 },
	{ NSDB_HOSTS, "gethostbyname2_r", netbsd_gethostbyname2_r, 0},
	{ NSDB_HOSTS, "getaddrinfo", netbsd_getaddrinfo, 0 },
	{ NSDB_HOSTS, "gethostbyaddr", netbsd_gethostbyaddr, 0 },
	{ NSDB_HOSTS, "gethostbyaddr_r", netbsd_gethostbyaddr_r, 0 },
};

static int
netbsd_gethostbyname(void *rv, void *cb_data, va_list ap)
{
	NSS_STATUS s;
	int err, herr;
	const char *name = va_arg(ap, char *);
	size_t namlen = va_arg(ap, size_t);
	int af = va_arg(ap, int);

	s = _nss_mdns_gethostbyname2_r(name, af, &host,
		hostbuf, sizeof(hostbuf), &err, &herr);

	if (s == NSS_STATUS_SUCCESS)
		*(struct hostent **)rv = &host;
	else {
		h_errno = HOST_NOT_FOUND;
		*(struct hostent **)rv = 0;
	}
	return nss2netbsderr[s];
}

static int
netbsd_gethostbyname2_r(void *rv, void *cb_data, va_list ap)
{
        NSS_STATUS s;
        int err, herr;
	char *buf;
	size_t buflen;
	struct hostent *hostentp;
	struct hostent **result_hostent;
        const char *name = va_arg(ap, char *);
        int af = va_arg(ap, int);

	hostentp = va_arg(ap, struct hostent *);
	buf = va_arg(ap, char *);
	buflen = va_arg(ap, size_t);
	err = va_arg(ap, int);
	herr = va_arg(ap, int);
	result_hostent = (struct hostent  **)rv;
	*result_hostent = NULL;

	if (hostentp == NULL)
		return (NS_UNAVAIL);

        s = _nss_mdns_gethostbyname2_r(name, af, hostentp,
                buf, buflen, &err, &herr);

        if (s == NSS_STATUS_SUCCESS)
                *result_hostent = hostentp;
        else {
                h_errno = HOST_NOT_FOUND;
                *result_hostent = NULL;
        }
        return nss2netbsderr[s];
}


static void
aiforaf(const char *name, int af, struct addrinfo *pai, struct addrinfo **aip)
{
	NSS_STATUS s;
	int err, herr;
	char **addrp;
	char addrstr[INET6_ADDRSTRLEN];
	struct addrinfo hints, *res0, *res;

	s = _nss_mdns_gethostbyname2_r(name, af, &host,
		hostbuf, sizeof(hostbuf), &err, &herr);
	if (s != NSS_STATUS_SUCCESS)
		return;

	for (addrp = host.h_addr_list; *addrp; addrp++) {
		/* XXX this sucks, but get_ai is not public */
		if (!inet_ntop(host.h_addrtype, *addrp,
			       addrstr, sizeof(addrstr)))
			continue;
		hints = *pai;
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_family = af;
		if (getaddrinfo(addrstr, NULL, &hints, &res0))
			continue;
		for (res = res0; res; res = res->ai_next)
			res->ai_flags = pai->ai_flags;

		(*aip)->ai_next = res0;
		while ((*aip)->ai_next)
			*aip = (*aip)->ai_next;
	}
}

static int
netbsd_getaddrinfo(void *rv, void *cb_data, va_list ap)
{
	struct addrinfo sentinel, *cur;
	const char *name = va_arg(ap, char *);
	struct addrinfo *ai = va_arg(ap, struct addrinfo *);

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	if ((ai->ai_family == AF_UNSPEC) || (ai->ai_family == AF_INET6))
		aiforaf(name, AF_INET6, ai, &cur);
	if ((ai->ai_family == AF_UNSPEC) || (ai->ai_family == AF_INET))
		aiforaf(name, AF_INET, ai, &cur);

	if (!sentinel.ai_next) {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	*((struct addrinfo **)rv) = sentinel.ai_next;
	return NS_SUCCESS;
}

static int
netbsd_gethostbyaddr(void *rv, void *cb_data, va_list ap)
{
	int err, herr;
	NSS_STATUS s;
	const unsigned char *addr = va_arg(ap, unsigned char *);
	socklen_t addrlen = va_arg(ap, socklen_t);
	int af = va_arg(ap, int);

	s = _nss_mdns_gethostbyaddr_r(addr, addrlen, af, &host,
		hostbuf, sizeof(hostbuf), &err, &herr);

	if (s == NSS_STATUS_SUCCESS)
		*(struct hostent **)rv = &host;
	else {
		h_errno = HOST_NOT_FOUND;
		*(struct hostent **)rv = 0;
	}
	return nss2netbsderr[s];
}

static int
netbsd_gethostbyaddr_r(void *rv, void *cb_data, va_list ap)
{
        int err;
	int *herrp;
        NSS_STATUS s;
	char *buf;
	size_t buflen;
	struct hostent *hostentp;
	struct hostent **result_hostent;
        const unsigned char *addr = va_arg(ap, unsigned char *);
        socklen_t addrlen = va_arg(ap, socklen_t);
        int af = va_arg(ap, int);

	hostentp = va_arg(ap, struct hostent *);
	buf = va_arg(ap, char *);
	buflen = va_arg(ap, size_t);
	err = va_arg(ap, int);
	herrp = va_arg(ap, int *);
	result_hostent = (struct hostent **)rv;

        s = _nss_mdns_gethostbyaddr_r(addr, addrlen, af, hostentp,
                buf, buflen, &err, herrp);

        if (s == NSS_STATUS_SUCCESS)
                *result_hostent = hostentp;
        else {
                h_errno = HOST_NOT_FOUND;
                *result_hostent = NULL;
        }
        return nss2netbsderr[s];
}


ns_mtab *
nss_module_register(const char *source, unsigned int *mtabsize,
		    nss_module_unregister_fn *unreg)
{

	*mtabsize = sizeof(methods)/sizeof(methods[0]);
	*unreg = NULL;
	return (methods);
}
