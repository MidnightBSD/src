/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <dns_sd.h>
#include <errno.h>
#include <netdb.h>
#include <nsswitch.h>
#include <poll.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef QUERY_TIMEOUT_MS
#define QUERY_TIMEOUT_MS 5000
#endif
#define MAX_RECORDS 128

struct answer {
	struct answer *next;
	uint16_t type;
	unsigned char address[NS_IN6ADDRSZ];
	char name[NS_MAXDNAME];
};

struct query {
	DNSServiceRef ref;
	struct answer *answers;
	struct answer **tail;
	uint16_t type;
	unsigned int count;
	int status;
	bool done;
};

static int dnssd_getaddrinfo(void *, void *, va_list);
static int dnssd_gethostbyname(void *, void *, va_list);
static int dnssd_gethostbyaddr(void *, void *, va_list);

static ns_mtab methods[] = {
	{ NSDB_HOSTS, "getaddrinfo", dnssd_getaddrinfo, NULL },
	{ NSDB_HOSTS, "gethostbyname2_r", dnssd_gethostbyname, NULL },
	{ NSDB_HOSTS, "gethostbyaddr_r", dnssd_gethostbyaddr, NULL },
};

ns_mtab *nss_module_register(const char *, unsigned int *,
    nss_module_unregister_fn *);

static void
free_answers(struct answer *a)
{
	struct answer *next;

	while (a != NULL) {
		next = a->next;
		free(a);
		a = next;
	}
}

/* PTR RDATA is an uncompressed DNS wire-format name, not a C string. */
static bool
ptr_name(const unsigned char *data, size_t len, char *name, size_t size)
{
	size_t n, used = 0;

	while (len != 0) {
		n = *data++;
		len--;
		if (n == 0) {
			if (len != 0 || used == 0)
				return (false);
			name[used - 1] = '\0';
			return (true);
		}
		if (n > 63 || n > len || n + 1 >= size - used)
			return (false);
		/* Host names cannot contain embedded separators or NULs. */
		for (size_t i = 0; i < n; i++) {
			if (data[i] <= ' ' || data[i] >= 127 ||
			    data[i] == '.' || data[i] == '\\')
				return (false);
		}
		memcpy(name + used, data, n);
		used += n;
		name[used++] = '.';
		data += n;
		len -= n;
	}
	return (false);
}

static void DNSSD_API
record_reply(DNSServiceRef ref __unused, DNSServiceFlags flags,
    uint32_t interface __unused, DNSServiceErrorType error,
    const char *fullname, uint16_t type, uint16_t class, uint16_t len,
    const void *data, uint32_t ttl __unused, void *context)
{
	struct query *q = context;
	struct answer *a;
	size_t n;

	if (q->done)
		return;
	if (error != kDNSServiceErr_NoError) {
		switch (error) {
		case kDNSServiceErr_NoSuchRecord:
			q->status = NS_NOTFOUND;
			break;
		case kDNSServiceErr_Timeout:
			q->status = NS_TRYAGAIN;
			break;
		default:
			q->status = NS_UNAVAIL;
			break;
		}
		q->done = true;
		return;
	}
	/* ReturnIntermediates also reports CNAMEs; wait for the final type. */
	if (type != q->type || class != kDNSServiceClass_IN ||
	    (flags & kDNSServiceFlagsAdd) == 0) {
		if (q->status == NS_SUCCESS &&
		    (flags & kDNSServiceFlagsMoreComing) == 0)
			q->done = true;
		return;
	}
	if (data == NULL || fullname == NULL ||
	    (type != kDNSServiceType_A && type != kDNSServiceType_AAAA &&
		type != kDNSServiceType_PTR) ||
	    (type == kDNSServiceType_A && len != NS_INADDRSZ) ||
	    (type == kDNSServiceType_AAAA && len != NS_IN6ADDRSZ) ||
	    q->count == MAX_RECORDS)
		goto invalid;
	a = calloc(1, sizeof(*a));
	if (a == NULL)
		goto invalid;
	a->type = type;
	if (type == kDNSServiceType_PTR) {
		if (!ptr_name(data, len, a->name, sizeof(a->name))) {
			free(a);
			goto invalid;
		}
	} else {
		memcpy(a->address, data, len);
		n = strlcpy(a->name, fullname, sizeof(a->name));
		if (n == 0 || n >= sizeof(a->name)) {
			free(a);
			goto invalid;
		}
		if (a->name[n - 1] == '.')
			a->name[n - 1] = '\0';
	}
	if (a->name[0] == '\0' || !res_hnok(a->name)) {
		free(a);
		goto invalid;
	}
	*q->tail = a;
	q->tail = &a->next;
	q->count++;
	q->status = NS_SUCCESS;
	q->done = (flags & kDNSServiceFlagsMoreComing) == 0;
	return;
invalid:
	q->status = NS_UNAVAIL;
	q->done = true;
}

static int64_t
milliseconds(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return (-1);
	return ((int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/*
 * Each family has its own completion state; MoreComing describes one batch.
 */
static int
query_name(const char *name, const uint16_t *types, size_t count,
    struct answer **result)
{
	struct query queries[2];
	struct pollfd fds[2];
	struct timeval timeout = { .tv_sec = QUERY_TIMEOUT_MS / 1000 };
	DNSServiceErrorType error;
	struct answer **tail;
	int64_t deadline, now;
	int n, status = NS_NOTFOUND;
	size_t i;
	bool pending;

	*result = NULL;
	memset(queries, 0, sizeof(queries));
	if (count == 0 || count > nitems(queries) ||
	    (issetugid() && getenv("DNSSD_UDS_PATH") != NULL))
		return (NS_UNAVAIL);
	now = milliseconds();
	if (now < 0)
		return (NS_UNAVAIL);
	deadline = now + QUERY_TIMEOUT_MS;
	for (i = 0; i < count; i++) {
		queries[i].type = types[i];
		queries[i].tail = &queries[i].answers;
		queries[i].status = NS_TRYAGAIN;
		error = DNSServiceQueryRecord(&queries[i].ref,
		    kDNSServiceFlagsReturnIntermediates |
			kDNSServiceFlagsTimeout,
		    kDNSServiceInterfaceIndexAny, name, types[i],
		    kDNSServiceClass_IN, record_reply, &queries[i]);
		if (error != kDNSServiceErr_NoError) {
			status = NS_UNAVAIL;
			goto out;
		}
		fds[i].fd = DNSServiceRefSockFD(queries[i].ref);
		fds[i].events = POLLIN;
		/* Limit stalls while reading incomplete IPC messages. */
		if (fds[i].fd < 0 ||
		    setsockopt(fds[i].fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
			sizeof(timeout)) != 0) {
			status = NS_UNAVAIL;
			goto out;
		}
	}
	for (;;) {
		pending = false;
		for (i = 0; i < count; i++) {
			if (queries[i].done)
				fds[i].fd = -1;
			fds[i].events = POLLIN;
			pending |= !queries[i].done;
		}
		if (!pending)
			break;
		now = milliseconds();
		if (now < 0 || now >= deadline)
			break;
		n = poll(fds, count, (int)(deadline - now));
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		for (i = 0; i < count; i++) {
			if (queries[i].done || fds[i].revents == 0)
				continue;
			now = milliseconds();
			if (now < 0 || now >= deadline)
				break;
			timeout.tv_sec = (deadline - now) / 1000;
			timeout.tv_usec = ((deadline - now) % 1000) * 1000;
			if ((fds[i].revents & POLLIN) == 0 ||
			    setsockopt(fds[i].fd, SOL_SOCKET, SO_RCVTIMEO,
				&timeout, sizeof(timeout)) != 0) {
				queries[i].status = NS_UNAVAIL;
				queries[i].done = true;
				continue;
			}
			error = DNSServiceProcessResult(queries[i].ref);
			/*
			 * Preserve a complete answer if IPC closes afterward.
			 */
			if (error != kDNSServiceErr_NoError &&
			    !queries[i].done) {
				queries[i].status = NS_UNAVAIL;
				queries[i].done = true;
			}
		}
	}
	for (i = 0; i < count; i++) {
		if (!queries[i].done)
			queries[i].status = NS_TRYAGAIN;
		if (queries[i].status == NS_UNAVAIL)
			status = NS_UNAVAIL;
		else if (queries[i].status == NS_TRYAGAIN &&
		    status != NS_UNAVAIL)
			status = NS_TRYAGAIN;
	}
	/*
	 * A completed family can still succeed if the other family fails.
	 */
	for (i = 0; i < count; i++) {
		if (queries[i].status == NS_SUCCESS && queries[i].done)
			status = NS_SUCCESS;
	}
out:
	tail = result;
	for (i = 0; i < count; i++) {
		if (queries[i].ref != NULL)
			DNSServiceRefDeallocate(queries[i].ref);
		if (status == NS_SUCCESS && queries[i].status == NS_SUCCESS &&
		    queries[i].done) {
			*tail = queries[i].answers;
			tail = queries[i].tail;
		} else
			free_answers(queries[i].answers);
	}
	return (status);
}

static bool
local_name(const char *name)
{
	size_t n = strlen(name);

	if (n != 0 && name[n - 1] == '.')
		n--;
	return ((n == 5 && strncasecmp(name, "local", n) == 0) ||
	    (n > 6 && strncasecmp(name + n - 6, ".local", 6) == 0));
}

/* Match resolv.conf's search order without sending queries from libc. */
static int
lookup(const char *name, const uint16_t *types, size_t count,
    struct answer **result)
{
	struct __res_state res;
	char candidate[NS_MAXDNAME], alias[NS_MAXDNAME];
	const char *domain, *suffix;
	size_t len, dots = 0;
	int status = NS_NOTFOUND, saved = NS_NOTFOUND, n;
	bool absolute, tried, root, root_on_list = false, searched = false;

	*result = NULL;
	len = strlen(name);
	if (len == 0 || len >= sizeof(candidate) - 1 || !res_hnok(name) ||
	    local_name(name))
		return (NS_UNAVAIL);
	for (size_t i = 0; i < len; i++)
		dots += name[i] == '.';
	absolute = name[len - 1] == '.';
	memset(&res, 0, sizeof(res));
	res._vcsock = -1;
	if (res_ninit(&res) != 0) {
		res_ndestroy(&res);
		return (NS_UNAVAIL);
	}
	if (dots == 0 &&
	    res_hostalias(&res, name, alias, sizeof(alias)) != NULL) {
		if (!res_hnok(alias) || local_name(alias))
			status = NS_UNAVAIL;
		else {
			snprintf(candidate, sizeof(candidate), "%s%s", alias,
			    alias[strlen(alias) - 1] == '.' ? "" : ".");
			status = query_name(candidate, types, count, result);
		}
		goto out;
	}
	tried = absolute || dots >= res.ndots;
	if (tried) {
		snprintf(candidate, sizeof(candidate), "%s%s", name,
		    absolute ? "" : ".");
		status = query_name(candidate, types, count, result);
		if (status != NS_NOTFOUND || absolute)
			goto out;
		saved = status;
	}
	if ((dots == 0 && (res.options & RES_DEFNAMES) != 0) ||
	    (dots != 0 && (res.options & RES_DNSRCH) != 0)) {
		for (size_t i = 0; res.dnsrch[i] != NULL; i++) {
			searched = true;
			domain = res.dnsrch[i];
			root = domain[0] == '\0' || strcmp(domain, ".") == 0;
			root_on_list |= root;
			if (root && tried)
				continue;
			suffix = root || domain[strlen(domain) - 1] == '.' ?
			    "" :
			    ".";
			if (root)
				n = snprintf(candidate, sizeof(candidate),
				    "%s.", name);
			else
				n = snprintf(candidate, sizeof(candidate),
				    "%s.%s%s", name, domain, suffix);
			if (n < 0 || n >= (int)sizeof(candidate))
				continue;
			if (!res_hnok(candidate) || local_name(candidate)) {
				status = NS_UNAVAIL;
				goto out;
			}
			status = query_name(candidate, types, count, result);
			if (status != NS_NOTFOUND)
				goto out;
			if ((res.options & RES_DNSRCH) == 0)
				break;
		}
	}
	if (!tried && !root_on_list &&
	    (dots != 0 || !searched || (res.options & RES_NOTLDQUERY) == 0)) {
		snprintf(candidate, sizeof(candidate), "%s.", name);
		status = query_name(candidate, types, count, result);
	} else if (tried)
		status = saved;
out:
	res_ndestroy(&res);
	return (status);
}

static int
dnssd_getaddrinfo(void *retval, void *mdata __unused, va_list ap)
{
	const char *name = va_arg(ap, const char *);
	const struct addrinfo *hints = va_arg(ap, const struct addrinfo *);
	struct answer *answers, *a;
	struct addrinfo *head = NULL, **tail = &head, *ai;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	uint16_t types[2];
	size_t count = 0, size;
	int status, family;
	bool have6 = false, mapped;

	*(struct addrinfo **)retval = NULL;
	if (hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET6)
		types[count++] = kDNSServiceType_AAAA;
	if (hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET ||
	    (hints->ai_family == AF_INET6 && (hints->ai_flags & AI_V4MAPPED)))
		types[count++] = kDNSServiceType_A;
	if (count == 0)
		return (NS_UNAVAIL);
	status = lookup(name, types, count, &answers);
	if (status != NS_SUCCESS)
		return (status);
	for (a = answers; a != NULL; a = a->next)
		have6 |= a->type == kDNSServiceType_AAAA;
	for (a = answers; a != NULL; a = a->next) {
		mapped = a->type == kDNSServiceType_A &&
		    hints->ai_family == AF_INET6;
		if (mapped && have6 && (hints->ai_flags & AI_ALL) == 0)
			continue;
		family = a->type == kDNSServiceType_AAAA || mapped ? AF_INET6 :
								     AF_INET;
		size = family == AF_INET ? sizeof(*sin) : sizeof(*sin6);
		ai = calloc(1, sizeof(*ai) + size);
		if (ai == NULL)
			goto nomem;
		*ai = *hints;
		ai->ai_next = NULL;
		ai->ai_canonname = NULL;
		ai->ai_family = family;
		ai->ai_addrlen = size;
		ai->ai_addr = (struct sockaddr *)(ai + 1);
		if (family == AF_INET) {
			sin = (struct sockaddr_in *)(void *)ai->ai_addr;
			sin->sin_len = sizeof(*sin);
			sin->sin_family = AF_INET;
			memcpy(&sin->sin_addr, a->address, NS_INADDRSZ);
		} else {
			sin6 = (struct sockaddr_in6 *)(void *)ai->ai_addr;
			sin6->sin6_len = sizeof(*sin6);
			sin6->sin6_family = AF_INET6;
			if (mapped) {
				sin6->sin6_addr.s6_addr[10] = 0xff;
				sin6->sin6_addr.s6_addr[11] = 0xff;
				memcpy(&sin6->sin6_addr.s6_addr[12], a->address,
				    NS_INADDRSZ);
			} else
				memcpy(&sin6->sin6_addr, a->address,
				    NS_IN6ADDRSZ);
		}
		*tail = ai;
		tail = &ai->ai_next;
		if (head == ai && (hints->ai_flags & AI_CANONNAME) != 0) {
			ai->ai_canonname = strdup(a->name);
			if (ai->ai_canonname == NULL)
				goto nomem;
		}
	}
	free_answers(answers);
	*(struct addrinfo **)retval = head;
	return (NS_SUCCESS);
nomem:
	free_answers(answers);
	freeaddrinfo(head);
	return (NS_UNAVAIL);
}

static int
host_result(void *retval, struct hostent *hp, char *buf, size_t buflen,
    int *errnop, int *herrnop, struct answer *answers, int af,
    const void *reverse)
{
	struct answer *a;
	char **addresses, **aliases, *cursor;
	const char *name;
	size_t count = 0, size, padding, needed, namesize;

	if (answers == NULL) {
		*errnop = EIO;
		*herrnop = NETDB_INTERNAL;
		return (NS_UNAVAIL);
	}
	name = answers->name;
	for (a = answers; a != NULL; a = a->next)
		count++;
	if (reverse != NULL)
		count = 1;
	size = af == AF_INET ? NS_INADDRSZ : NS_IN6ADDRSZ;
	padding = (-(uintptr_t)buf) & (sizeof(char *) - 1);
	namesize = strlen(answers->name) + 1;
	needed = padding + (count + 2) * sizeof(char *) + count * size +
	    namesize;
	if (needed > buflen) {
		*errnop = ERANGE;
		*herrnop = NETDB_INTERNAL;
		return (NS_RETURN);
	}
	addresses = (char **)(void *)(buf + padding);
	aliases = addresses + count + 1;
	*aliases = NULL;
	cursor = (char *)(aliases + 1);
	for (size_t i = 0; i < count; i++) {
		addresses[i] = cursor;
		memcpy(cursor, reverse != NULL ? reverse : answers->address,
		    size);
		cursor += size;
		if (reverse == NULL)
			answers = answers->next;
	}
	addresses[count] = NULL;
	memcpy(cursor, name, namesize);
	hp->h_name = cursor;
	hp->h_aliases = aliases;
	hp->h_addrtype = af;
	hp->h_length = size;
	hp->h_addr_list = addresses;
	*errnop = 0;
	*herrnop = NETDB_SUCCESS;
	*(struct hostent **)retval = hp;
	return (NS_SUCCESS);
}

static int
host_status(int status, int *errnop, int *herrnop)
{
	*errnop = status == NS_UNAVAIL ? EIO : 0;
	if (status == NS_NOTFOUND)
		*herrnop = HOST_NOT_FOUND;
	else if (status == NS_TRYAGAIN)
		*herrnop = TRY_AGAIN;
	else
		*herrnop = NETDB_INTERNAL;
	return (status);
}

static int
dnssd_gethostbyname(void *retval, void *mdata __unused, va_list ap)
{
	const char *name = va_arg(ap, const char *);
	int af = va_arg(ap, int);
	struct hostent *hp = va_arg(ap, struct hostent *);
	char *buf = va_arg(ap, char *);
	size_t buflen = va_arg(ap, size_t);
	int *errnop = va_arg(ap, int *);
	int *herrnop = va_arg(ap, int *);
	struct answer *answers;
	uint16_t type;
	int status;

	*(struct hostent **)retval = NULL;
	if (af != AF_INET && af != AF_INET6)
		return (host_status(NS_UNAVAIL, errnop, herrnop));
	type = af == AF_INET ? kDNSServiceType_A : kDNSServiceType_AAAA;
	status = lookup(name, &type, 1, &answers);
	if (status != NS_SUCCESS)
		return (host_status(status, errnop, herrnop));
	status = host_result(retval, hp, buf, buflen, errnop, herrnop, answers,
	    af, NULL);
	free_answers(answers);
	return (status);
}

static int
dnssd_gethostbyaddr(void *retval, void *mdata __unused, va_list ap)
{
	const unsigned char *addr = va_arg(ap, const void *);
	socklen_t len = va_arg(ap, socklen_t);
	int af = va_arg(ap, int);
	struct hostent *hp = va_arg(ap, struct hostent *);
	char *buf = va_arg(ap, char *);
	size_t buflen = va_arg(ap, size_t);
	int *errnop = va_arg(ap, int *);
	int *herrnop = va_arg(ap, int *);
	char name[NS_MAXDNAME], *p;
	struct answer *answers;
	uint16_t type = kDNSServiceType_PTR;
	int status;

	*(struct hostent **)retval = NULL;
	if ((af == AF_INET && len == NS_INADDRSZ && addr[0] == 169 &&
		addr[1] == 254) ||
	    (af == AF_INET6 && len == NS_IN6ADDRSZ && addr[0] == 0xfe &&
		(addr[1] & 0xc0) == 0x80))
		return (host_status(NS_UNAVAIL, errnop, herrnop));
	if (af == AF_INET && len == NS_INADDRSZ)
		snprintf(name, sizeof(name), "%u.%u.%u.%u.in-addr.arpa.",
		    addr[3], addr[2], addr[1], addr[0]);
	else if (af == AF_INET6 && len == NS_IN6ADDRSZ) {
		p = name;
		for (int i = NS_IN6ADDRSZ - 1; i >= 0; i--) {
			*p++ = "0123456789abcdef"[addr[i] & 15];
			*p++ = '.';
			*p++ = "0123456789abcdef"[addr[i] >> 4];
			*p++ = '.';
		}
		strlcpy(p, "ip6.arpa.", sizeof(name) - (size_t)(p - name));
	} else
		return (host_status(NS_UNAVAIL, errnop, herrnop));
	status = query_name(name, &type, 1, &answers);
	if (status != NS_SUCCESS)
		return (host_status(status, errnop, herrnop));
	status = host_result(retval, hp, buf, buflen, errnop, herrnop, answers,
	    af, addr);
	free_answers(answers);
	return (status);
}

ns_mtab *
nss_module_register(const char *source __unused, unsigned int *size,
    nss_module_unregister_fn *unregister)
{
	*size = nitems(methods);
	*unregister = NULL;
	return (methods);
}
