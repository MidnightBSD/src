/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project.
 */

#include <sys/types.h>

#include <netinet/in.h>

#include <atf-c.h>
#include <resolv.h>
#include <stdio.h>

#undef res_ninit
#undef res_ndestroy
#undef res_hostalias
#define res_ninit mock_res_ninit
#define res_ndestroy mock_res_ndestroy
#define res_hostalias mock_res_hostalias

static int mock_res_ninit(res_state);
static void mock_res_ndestroy(res_state);
static const char *mock_res_hostalias(const res_state, const char *, char *,
    size_t);

#define issetugid mock_issetugid
static int mock_issetugid(void);
static int privileged;

#define QUERY_TIMEOUT_MS 50
#include "../nss_dnssd.c"

enum scenario {
	DUAL,
	V4_ONLY,
	NEGATIVE,
	CREATE_FAILURE,
	MALFORMED,
	CNAME,
	SILENT,
	PARTIAL,
	PROCESS_FAILURE,
	REVERSE,
	BAD_PTR,
	BAD_HOSTNAME,
	UNFINISHED,
	CLOSED_AFTER_REPLY
};

struct _DNSServiceRef_t {
	int pair[2];
	DNSServiceQueryRecordReply callback;
	void *context;
	uint16_t type;
	unsigned int step;
};

static enum scenario scenario;
static unsigned int created, released, calls;
static char queried[NS_MAXDNAME];
static char history[16][NS_MAXDNAME];
static char *search_domain;
static const char *alias_name;
static unsigned int resolver_destroyed;

static int
mock_issetugid(void)
{
	return (privileged);
}

static int
mock_res_ninit(res_state res)
{
	res->ndots = 1;
	res->options = RES_DEFNAMES | RES_DNSRCH;
	res->dnsrch[0] = search_domain;
	res->dnsrch[1] = NULL;
	return (0);
}

static void
mock_res_ndestroy(res_state res __unused)
{
	resolver_destroyed++;
}

static const char *
mock_res_hostalias(const res_state res __unused, const char *name __unused,
    char *buf, size_t len)
{
	if (alias_name == NULL)
		return (NULL);
	strlcpy(buf, alias_name, len);
	return (buf);
}

static void
reset(enum scenario next)
{
	scenario = next;
	created = released = calls = 0;
	queried[0] = '\0';
	search_domain = NULL;
	alias_name = NULL;
	resolver_destroyed = 0;
	memset(history, 0, sizeof(history));
}

DNSServiceErrorType DNSSD_API
DNSServiceQueryRecord(DNSServiceRef *ref, DNSServiceFlags flags,
    uint32_t interface, const char *name, uint16_t type, uint16_t class,
    DNSServiceQueryRecordReply callback, void *context)
{
	DNSServiceRef r;

	ATF_REQUIRE(calls < nitems(history));
	strlcpy(history[calls], name, sizeof(history[calls]));
	calls++;
	ATF_CHECK((flags & kDNSServiceFlagsForceMulticast) == 0);
	ATF_CHECK((flags & kDNSServiceFlagsReturnIntermediates) != 0);
	ATF_CHECK_EQ(interface, kDNSServiceInterfaceIndexAny);
	ATF_CHECK_EQ(class, kDNSServiceClass_IN);
	if (scenario == CREATE_FAILURE && calls == 2)
		return (kDNSServiceErr_ServiceNotRunning);
	r = calloc(1, sizeof(*r));
	ATF_REQUIRE(r != NULL);
	ATF_REQUIRE_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, r->pair), 0);
	r->callback = callback;
	r->context = context;
	r->type = type;
	strlcpy(queried, name, sizeof(queried));
	if (scenario != SILENT &&
	    !(scenario == PARTIAL && type == kDNSServiceType_AAAA))
		ATF_REQUIRE_EQ(write(r->pair[1], "x", 1), 1);
	*ref = r;
	created++;
	return (kDNSServiceErr_NoError);
}

int DNSSD_API
DNSServiceRefSockFD(DNSServiceRef ref)
{
	return (ref->pair[0]);
}

void DNSSD_API
DNSServiceRefDeallocate(DNSServiceRef ref)
{
	ATF_CHECK_EQ(close(ref->pair[0]), 0);
	ATF_CHECK_EQ(close(ref->pair[1]), 0);
	free(ref);
	released++;
}

DNSServiceErrorType DNSSD_API
DNSServiceProcessResult(DNSServiceRef ref)
{
	static const unsigned char ptr[] = { 4, 'h', 'o', 's', 't', 7, 'e', 'x',
		'a', 'm', 'p', 'l', 'e', 0 };
	unsigned char address[NS_IN6ADDRSZ] = { 0 };
	DNSServiceFlags flags = kDNSServiceFlagsAdd;
	uint16_t type = ref->type, len;
	char byte;

	ATF_REQUIRE_EQ(read(ref->pair[0], &byte, 1), 1);
	if (scenario == PROCESS_FAILURE)
		return (kDNSServiceErr_Unknown);
	if (scenario == NEGATIVE ||
	    (scenario == V4_ONLY && type == kDNSServiceType_AAAA)) {
		/*
		 * Error callbacks leave the remaining parameters undefined.
		 */
		ref->callback(ref, 0, 0, kDNSServiceErr_NoSuchRecord, NULL, 0,
		    0, 0, NULL, 0, ref->context);
		return (kDNSServiceErr_NoError);
	}
	if (type == kDNSServiceType_PTR) {
		ref->callback(ref, flags, 0, kDNSServiceErr_NoError, queried,
		    type, kDNSServiceClass_IN,
		    scenario == BAD_PTR ? sizeof(ptr) - 1 : sizeof(ptr), ptr,
		    60, ref->context);
		return (kDNSServiceErr_NoError);
	}
	len = type == kDNSServiceType_A ? NS_INADDRSZ : NS_IN6ADDRSZ;
	ATF_REQUIRE_EQ(inet_pton(type == kDNSServiceType_A ? AF_INET : AF_INET6,
			   type == kDNSServiceType_A ? "192.0.2.1" :
						       "2001:db8::1",
			   address),
	    1);
	if (scenario == CNAME && ref->step++ == 0) {
		type = kDNSServiceType_CNAME;
		ATF_REQUIRE_EQ(write(ref->pair[1], "x", 1), 1);
	} else if (scenario == DUAL && ref->type == kDNSServiceType_A &&
	    ref->step++ == 0) {
		flags |= kDNSServiceFlagsMoreComing;
		ATF_REQUIRE_EQ(write(ref->pair[1], "x", 1), 1);
	} else if (scenario == DUAL && ref->type == kDNSServiceType_A)
		address[3] = 2;
	if (scenario == UNFINISHED)
		flags |= kDNSServiceFlagsMoreComing;
	ref->callback(ref, flags, 0, kDNSServiceErr_NoError,
	    scenario == BAD_HOSTNAME ? "bad/name.example." :
				       "canonical.example.",
	    type, kDNSServiceClass_IN, scenario == MALFORMED ? len - 1 : len,
	    address, 60, ref->context);
	if (scenario == CLOSED_AFTER_REPLY) {
		ref->callback(ref, 0, 0, kDNSServiceErr_ServiceNotRunning, NULL,
		    0, 0, 0, NULL, 0, ref->context);
		return (kDNSServiceErr_ServiceNotRunning);
	}
	return (kDNSServiceErr_NoError);
}

static int
invoke(nss_method method, void *result, ...)
{
	va_list ap;
	int status;

	va_start(ap, result);
	status = method(result, NULL, ap);
	va_end(ap);
	return (status);
}

static struct addrinfo *
addresses(int family, int flags)
{
	struct addrinfo hints = { .ai_family = family,
		.ai_flags = flags,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP };
	struct addrinfo *result = NULL;

	ATF_REQUIRE_EQ(invoke(dnssd_getaddrinfo, &result, "host.example.",
			   &hints),
	    NS_SUCCESS);
	ATF_REQUIRE(result != NULL);
	ATF_CHECK_EQ(created, released);
	return (result);
}

ATF_TC_WITHOUT_HEAD(dual_stack);
ATF_TC_BODY(dual_stack, tc)
{
	struct addrinfo *result, *ai;
	unsigned int v4 = 0, v6 = 0;

	reset(DUAL);
	result = addresses(AF_UNSPEC, AI_CANONNAME);
	ATF_CHECK_STREQ(result->ai_canonname, "canonical.example");
	for (ai = result; ai != NULL; ai = ai->ai_next) {
		ATF_CHECK_EQ(ai->ai_socktype, SOCK_STREAM);
		ATF_CHECK_EQ(ai->ai_protocol, IPPROTO_TCP);
		ATF_CHECK_EQ(ai->ai_flags, AI_CANONNAME);
		ATF_CHECK(ai->ai_addr == (struct sockaddr *)(ai + 1));
		ATF_CHECK_EQ(ai->ai_addr->sa_len, ai->ai_addrlen);
		v4 += ai->ai_family == AF_INET;
		v6 += ai->ai_family == AF_INET6;
	}
	ATF_CHECK_EQ(v4, 2);
	ATF_CHECK_EQ(v6, 1);
	ATF_CHECK_EQ(created, 2);
	freeaddrinfo(result);
}

static bool
is_mapped(const struct addrinfo *ai)
{
	struct sockaddr_in6 address;

	ATF_REQUIRE_EQ(ai->ai_family, AF_INET6);
	ATF_REQUIRE_EQ(ai->ai_addrlen, sizeof(address));
	memcpy(&address, ai->ai_addr, sizeof(address));
	return (IN6_IS_ADDR_V4MAPPED(&address.sin6_addr));
}

ATF_TC_WITHOUT_HEAD(mapped_addresses);
ATF_TC_BODY(mapped_addresses, tc)
{
	struct addrinfo *result, *ai;
	unsigned int mapped;

	reset(DUAL);
	result = addresses(AF_INET6, AI_V4MAPPED);
	ATF_CHECK(result->ai_next == NULL);
	ATF_CHECK(!is_mapped(result));
	freeaddrinfo(result);
	reset(DUAL);
	result = addresses(AF_INET6, AI_V4MAPPED | AI_ALL);
	mapped = 0;
	for (ai = result; ai != NULL; ai = ai->ai_next) {
		ATF_CHECK_EQ(ai->ai_family, AF_INET6);
		mapped += is_mapped(ai);
	}
	ATF_CHECK_EQ(mapped, 2);
	freeaddrinfo(result);
	reset(V4_ONLY);
	result = addresses(AF_INET6, AI_V4MAPPED);
	ATF_CHECK(is_mapped(result));
	freeaddrinfo(result);
}

ATF_TC_WITHOUT_HEAD(host_buffers);
ATF_TC_BODY(host_buffers, tc)
{
	struct hostent host, *result = NULL;
	char storage[1025], *buf = storage + 1;
	size_t needed;
	int error = -1, herror = -1;

	reset(DUAL);
	ATF_REQUIRE_EQ(invoke(dnssd_gethostbyname, &result, "host.example.",
			   AF_INET, &host, buf, sizeof(storage) - 1, &error,
			   &herror),
	    NS_SUCCESS);
	ATF_CHECK(result == &host);
	ATF_CHECK_STREQ(host.h_name, "canonical.example");
	ATF_CHECK_EQ((uintptr_t)host.h_addr_list % sizeof(char *), 0);
	ATF_CHECK(host.h_addr_list[0] != NULL);
	ATF_CHECK(host.h_addr_list[1] != NULL);
	ATF_CHECK(host.h_addr_list[2] == NULL);
	ATF_CHECK(host.h_aliases[0] == NULL);
	ATF_CHECK_EQ(error, 0);
	ATF_CHECK_EQ(herror, NETDB_SUCCESS);
	ATF_CHECK_EQ(created, released);
	needed = (size_t)(host.h_name + strlen(host.h_name) + 1 - buf);
	reset(DUAL);
	ATF_REQUIRE_EQ(invoke(dnssd_gethostbyname, &result, "host.example.",
			   AF_INET, &host, buf, needed, &error, &herror),
	    NS_SUCCESS);
	ATF_CHECK_EQ(created, released);
	reset(DUAL);
	result = &host;
	ATF_CHECK_EQ(invoke(dnssd_gethostbyname, &result, "host.example.",
			 AF_INET, &host, buf, needed - 1, &error, &herror),
	    NS_RETURN);
	ATF_CHECK(result == NULL);
	ATF_CHECK_EQ(error, ERANGE);
	ATF_CHECK_EQ(herror, NETDB_INTERNAL);
	ATF_CHECK_EQ(created, released);
}

ATF_TC_WITHOUT_HEAD(reverse_queries);
ATF_TC_BODY(reverse_queries, tc)
{
	struct hostent host, *result;
	struct in_addr addr;
	struct in6_addr addr6;
	char buf[1024];
	int error, herror;

	reset(REVERSE);
	ATF_REQUIRE_EQ(inet_pton(AF_INET, "192.0.2.1", &addr), 1);
	ATF_REQUIRE_EQ(invoke(dnssd_gethostbyaddr, &result, &addr,
			   (socklen_t)sizeof(addr), AF_INET, &host, buf,
			   sizeof(buf), &error, &herror),
	    NS_SUCCESS);
	ATF_CHECK_STREQ(queried, "1.2.0.192.in-addr.arpa.");
	ATF_CHECK_STREQ(host.h_name, "host.example");
	ATF_CHECK_EQ(memcmp(host.h_addr_list[0], &addr, sizeof(addr)), 0);
	ATF_CHECK(host.h_addr_list[1] == NULL);
	ATF_CHECK_EQ(created, released);
	reset(REVERSE);
	ATF_REQUIRE_EQ(inet_pton(AF_INET6, "2001:db8::1", &addr6), 1);
	ATF_REQUIRE_EQ(invoke(dnssd_gethostbyaddr, &result, &addr6,
			   (socklen_t)sizeof(addr6), AF_INET6, &host, buf,
			   sizeof(buf), &error, &herror),
	    NS_SUCCESS);
	ATF_CHECK_STREQ(queried,
	    "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa.");
	ATF_CHECK_EQ(memcmp(host.h_addr_list[0], &addr6, sizeof(addr6)), 0);
	ATF_CHECK_EQ(created, released);
	reset(REVERSE);
	result = &host;
	ATF_CHECK_EQ(invoke(dnssd_gethostbyaddr, &result, &addr6,
			 (socklen_t)sizeof(addr6), AF_INET6, &host, buf,
			 (size_t)1, &error, &herror),
	    NS_RETURN);
	ATF_CHECK(result == NULL);
	ATF_CHECK_EQ(error, ERANGE);
	ATF_CHECK_EQ(created, released);
}

ATF_TC_WITHOUT_HEAD(failures);
ATF_TC_BODY(failures, tc)
{
	static const struct {
		enum scenario scenario;
		int status;
	} cases[] = { { NEGATIVE, NS_NOTFOUND }, { CREATE_FAILURE, NS_UNAVAIL },
		{ MALFORMED, NS_UNAVAIL }, { SILENT, NS_TRYAGAIN },
		{ PROCESS_FAILURE, NS_UNAVAIL }, { BAD_HOSTNAME, NS_UNAVAIL },
		{ UNFINISHED, NS_TRYAGAIN } };
	struct addrinfo hints = { .ai_family = AF_UNSPEC }, *result;

	for (size_t i = 0; i < nitems(cases); i++) {
		reset(cases[i].scenario);
		result = NULL;
		ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "host.example.",
				 &hints),
		    cases[i].status);
		ATF_CHECK(result == NULL);
		ATF_CHECK_EQ(created, released);
	}
}

ATF_TC_WITHOUT_HEAD(intermediate_and_partial);
ATF_TC_BODY(intermediate_and_partial, tc)
{
	struct addrinfo *result;

	reset(CNAME);
	result = addresses(AF_INET, AI_CANONNAME);
	ATF_CHECK_STREQ(result->ai_canonname, "canonical.example");
	ATF_CHECK(result->ai_next == NULL);
	freeaddrinfo(result);
	reset(PARTIAL);
	result = addresses(AF_UNSPEC, 0);
	ATF_CHECK_EQ(result->ai_family, AF_INET);
	ATF_CHECK(result->ai_next == NULL);
	freeaddrinfo(result);
}

ATF_TC_WITHOUT_HEAD(ptr_validation);
ATF_TC_BODY(ptr_validation, tc)
{
	struct hostent host, *result;
	struct in_addr addr = { .s_addr = 0 };
	char buf[1024];
	int error, herror;

	reset(BAD_PTR);
	ATF_CHECK_EQ(invoke(dnssd_gethostbyaddr, &result, &addr,
			 (socklen_t)sizeof(addr), AF_INET, &host, buf,
			 sizeof(buf), &error, &herror),
	    NS_UNAVAIL);
	ATF_CHECK(result == NULL);
	ATF_CHECK_EQ(created, released);
}

ATF_TC_WITHOUT_HEAD(ptr_wire_boundaries);
ATF_TC_BODY(ptr_wire_boundaries, tc)
{
	static const unsigned char valid[] = { 1, 'a', 0 };
	static const unsigned char root[] = { 0 };
	static const unsigned char compressed[] = { 0xc0, 0x0c };
	static const unsigned char embedded_dot[] = { 3, 'a', '.', 'b', 0 };
	static const unsigned char embedded_nul[] = { 3, 'a', 0, 'b', 0 };
	static const unsigned char extra[] = { 1, 'a', 0, 0 };
	char name[NS_MAXDNAME];

	ATF_REQUIRE(ptr_name(valid, sizeof(valid), name, sizeof(name)));
	ATF_CHECK_STREQ(name, "a");
	ATF_CHECK(!ptr_name(valid, sizeof(valid) - 1, name, sizeof(name)));
	ATF_CHECK(!ptr_name(valid, sizeof(valid), name, 1));
	ATF_CHECK(!ptr_name(root, sizeof(root), name, sizeof(name)));
	ATF_CHECK(
	    !ptr_name(compressed, sizeof(compressed), name, sizeof(name)));
	ATF_CHECK(
	    !ptr_name(embedded_dot, sizeof(embedded_dot), name, sizeof(name)));
	ATF_CHECK(
	    !ptr_name(embedded_nul, sizeof(embedded_nul), name, sizeof(name)));
	ATF_CHECK(!ptr_name(extra, sizeof(extra), name, sizeof(name)));
}

ATF_TC_WITHOUT_HEAD(search_policy);
ATF_TC_BODY(search_policy, tc)
{
	static char example[] = "example", local[] = "local";
	struct addrinfo hints = { .ai_family = AF_INET }, *result;

	reset(NEGATIVE);
	search_domain = example;
	ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "short", &hints),
	    NS_NOTFOUND);
	ATF_REQUIRE_EQ(calls, 2);
	ATF_CHECK_STREQ(history[0], "short.example.");
	ATF_CHECK_STREQ(history[1], "short.");
	ATF_CHECK_EQ(resolver_destroyed, 1);
	reset(NEGATIVE);
	search_domain = example;
	ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "host.test", &hints),
	    NS_NOTFOUND);
	ATF_REQUIRE_EQ(calls, 2);
	ATF_CHECK_STREQ(history[0], "host.test.");
	ATF_CHECK_STREQ(history[1], "host.test.example.");
	reset(NEGATIVE);
	search_domain = example;
	ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "host.test.", &hints),
	    NS_NOTFOUND);
	ATF_REQUIRE_EQ(calls, 1);
	reset(DUAL);
	alias_name = "aliased.example";
	ATF_REQUIRE_EQ(invoke(dnssd_getaddrinfo, &result, "short", &hints),
	    NS_SUCCESS);
	ATF_CHECK_STREQ(queried, "aliased.example.");
	ATF_CHECK_EQ(calls, 1);
	freeaddrinfo(result);
	reset(DUAL);
	search_domain = local;
	ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "printer", &hints),
	    NS_UNAVAIL);
	ATF_CHECK_EQ(calls, 0);
	ATF_CHECK_EQ(resolver_destroyed, 1);
}

ATF_TC_WITHOUT_HEAD(local_passthrough);
ATF_TC_BODY(local_passthrough, tc)
{
	struct addrinfo hints = { .ai_family = AF_INET }, *result;

	reset(DUAL);
	ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "printer.LOCAL.",
			 &hints),
	    NS_UNAVAIL);
	ATF_CHECK_EQ(calls, 0);
}

ATF_TC_WITHOUT_HEAD(answer_before_disconnect);
ATF_TC_BODY(answer_before_disconnect, tc)
{
	struct addrinfo *result;

	reset(CLOSED_AFTER_REPLY);
	result = addresses(AF_UNSPEC, AI_CANONNAME);
	ATF_CHECK_STREQ(result->ai_canonname, "canonical.example");
	ATF_REQUIRE(result->ai_next != NULL);
	ATF_CHECK(result->ai_next->ai_next == NULL);
	freeaddrinfo(result);
}

ATF_TC_WITHOUT_HEAD(privileged_socket);
ATF_TC_BODY(privileged_socket, tc)
{
	struct addrinfo hints = { .ai_family = AF_INET }, *result;

	reset(DUAL);
	privileged = 1;
	ATF_REQUIRE_EQ(setenv("DNSSD_UDS_PATH", "/test/socket", 1), 0);
	ATF_CHECK_EQ(invoke(dnssd_getaddrinfo, &result, "host.example.",
			 &hints),
	    NS_UNAVAIL);
	ATF_CHECK(result == NULL);
	ATF_CHECK_EQ(calls, 0);
	ATF_REQUIRE_EQ(unsetenv("DNSSD_UDS_PATH"), 0);
	result = addresses(AF_INET, 0);
	freeaddrinfo(result);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dual_stack);
	ATF_TP_ADD_TC(tp, mapped_addresses);
	ATF_TP_ADD_TC(tp, host_buffers);
	ATF_TP_ADD_TC(tp, reverse_queries);
	ATF_TP_ADD_TC(tp, failures);
	ATF_TP_ADD_TC(tp, intermediate_and_partial);
	ATF_TP_ADD_TC(tp, ptr_validation);
	ATF_TP_ADD_TC(tp, ptr_wire_boundaries);
	ATF_TP_ADD_TC(tp, local_passthrough);
	ATF_TP_ADD_TC(tp, search_policy);
	ATF_TP_ADD_TC(tp, privileged_socket);
	ATF_TP_ADD_TC(tp, answer_before_disconnect);
	return (atf_no_error());
}
