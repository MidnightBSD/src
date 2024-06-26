/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * Copyright (c) 2011 Pawel Jakub Dawidek <pawel@dawidek.net>
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>	/* MAXHOSTNAMELEN */
#include <sys/socket.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pjdlog.h"
#include "proto_impl.h"
#include "subr.h"

#define	TCP_CTX_MAGIC	0x7c41c
struct tcp_ctx {
	int			tc_magic;
	struct sockaddr_storage	tc_sa;
	int			tc_fd;
	int			tc_side;
#define	TCP_SIDE_CLIENT		0
#define	TCP_SIDE_SERVER_LISTEN	1
#define	TCP_SIDE_SERVER_WORK	2
};

static int tcp_connect_wait(void *ctx, int timeout);
static void tcp_close(void *ctx);

/*
 * Function converts the given string to unsigned number.
 */
static int
numfromstr(const char *str, intmax_t minnum, intmax_t maxnum, intmax_t *nump)
{
	intmax_t digit, num;

	if (str[0] == '\0')
		goto invalid;	/* Empty string. */
	num = 0;
	for (; *str != '\0'; str++) {
		if (*str < '0' || *str > '9')
			goto invalid;	/* Non-digit character. */
		digit = *str - '0';
		if (num > num * 10 + digit)
			goto invalid;	/* Overflow. */
		num = num * 10 + digit;
		if (num > maxnum)
			goto invalid;	/* Too big. */
	}
	if (num < minnum)
		goto invalid;	/* Too small. */
	*nump = num;
	return (0);
invalid:
	errno = EINVAL;
	return (-1);
}

static int
tcp_addr(const char *addr, int defport, struct sockaddr_storage *sap)
{
	char iporhost[MAXHOSTNAMELEN], portstr[6];
	struct addrinfo hints;
	struct addrinfo *res;
	const char *pp;
	intmax_t port;
	size_t size;
	int error;

	if (addr == NULL)
		return (-1);

	bzero(&hints, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (strncasecmp(addr, "tcp4://", 7) == 0) {
		addr += 7;
		hints.ai_family = PF_INET;
	} else if (strncasecmp(addr, "tcp6://", 7) == 0) {
		addr += 7;
		hints.ai_family = PF_INET6;
	} else if (strncasecmp(addr, "tcp://", 6) == 0) {
		addr += 6;
	} else {
		/*
		 * Because TCP is the default assume IP or host is given without
		 * prefix.
		 */
	}

	/*
	 * Extract optional port.
	 * There are three cases to consider.
	 * 1. hostname with port, eg. freefall.freebsd.org:8457
	 * 2. IPv4 address with port, eg. 192.168.0.101:8457
	 * 3. IPv6 address with port, eg. [fe80::1]:8457
	 * We discover IPv6 address by checking for two colons and if port is
	 * given, the address has to start with [.
	 */
	pp = NULL;
	if (strchr(addr, ':') != strrchr(addr, ':')) {
		if (addr[0] == '[')
			pp = strrchr(addr, ':');
	} else {
		pp = strrchr(addr, ':');
	}
	if (pp == NULL) {
		/* Port not given, use the default. */
		port = defport;
	} else {
		if (numfromstr(pp + 1, 1, 65535, &port) == -1)
			return (errno);
	}
	(void)snprintf(portstr, sizeof(portstr), "%jd", (intmax_t)port);
	/* Extract host name or IP address. */
	if (pp == NULL) {
		size = sizeof(iporhost);
		if (strlcpy(iporhost, addr, size) >= size)
			return (ENAMETOOLONG);
	} else if (addr[0] == '[' && pp[-1] == ']') {
		size = (size_t)(pp - addr - 2 + 1);
		if (size > sizeof(iporhost))
			return (ENAMETOOLONG);
		(void)strlcpy(iporhost, addr + 1, size);
	} else {
		size = (size_t)(pp - addr + 1);
		if (size > sizeof(iporhost))
			return (ENAMETOOLONG);
		(void)strlcpy(iporhost, addr, size);
	}

	error = getaddrinfo(iporhost, portstr, &hints, &res);
	if (error != 0) {
		pjdlog_debug(1, "getaddrinfo(%s, %s) failed: %s.", iporhost,
		    portstr, gai_strerror(error));
		return (EINVAL);
	}
	if (res == NULL)
		return (ENOENT);

	memcpy(sap, res->ai_addr, res->ai_addrlen);

	freeaddrinfo(res);

	return (0);
}

static int
tcp_setup_new(const char *addr, int side, void **ctxp)
{
	struct tcp_ctx *tctx;
	int ret, nodelay;

	PJDLOG_ASSERT(addr != NULL);
	PJDLOG_ASSERT(side == TCP_SIDE_CLIENT ||
	    side == TCP_SIDE_SERVER_LISTEN);
	PJDLOG_ASSERT(ctxp != NULL);

	tctx = malloc(sizeof(*tctx));
	if (tctx == NULL)
		return (errno);

	/* Parse given address. */
	if ((ret = tcp_addr(addr, PROTO_TCP_DEFAULT_PORT, &tctx->tc_sa)) != 0) {
		free(tctx);
		return (ret);
	}

	PJDLOG_ASSERT(tctx->tc_sa.ss_family != AF_UNSPEC);

	tctx->tc_fd = socket(tctx->tc_sa.ss_family, SOCK_STREAM, 0);
	if (tctx->tc_fd == -1) {
		ret = errno;
		free(tctx);
		return (ret);
	}

	PJDLOG_ASSERT(tctx->tc_sa.ss_family != AF_UNSPEC);

	/* Socket settings. */
	nodelay = 1;
	if (setsockopt(tctx->tc_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
	    sizeof(nodelay)) == -1) {
		pjdlog_errno(LOG_WARNING, "Unable to set TCP_NOELAY");
	}

	tctx->tc_side = side;
	tctx->tc_magic = TCP_CTX_MAGIC;
	*ctxp = tctx;

	return (0);
}

static int
tcp_setup_wrap(int fd, int side, void **ctxp)
{
	struct tcp_ctx *tctx;

	PJDLOG_ASSERT(fd >= 0);
	PJDLOG_ASSERT(side == TCP_SIDE_CLIENT ||
	    side == TCP_SIDE_SERVER_WORK);
	PJDLOG_ASSERT(ctxp != NULL);

	tctx = malloc(sizeof(*tctx));
	if (tctx == NULL)
		return (errno);

	tctx->tc_fd = fd;
	tctx->tc_sa.ss_family = AF_UNSPEC;
	tctx->tc_side = side;
	tctx->tc_magic = TCP_CTX_MAGIC;
	*ctxp = tctx;

	return (0);
}

static int
tcp_client(const char *srcaddr, const char *dstaddr, void **ctxp)
{
	struct tcp_ctx *tctx;
	struct sockaddr_storage sa;
	int ret;

	ret = tcp_setup_new(dstaddr, TCP_SIDE_CLIENT, ctxp);
	if (ret != 0)
		return (ret);
	tctx = *ctxp;
	if (srcaddr == NULL)
		return (0);
	ret = tcp_addr(srcaddr, 0, &sa);
	if (ret != 0) {
		tcp_close(tctx);
		return (ret);
	}
	if (bind(tctx->tc_fd, (struct sockaddr *)&sa, sa.ss_len) == -1) {
		ret = errno;
		tcp_close(tctx);
		return (ret);
	}
	return (0);
}

static int
tcp_connect(void *ctx, int timeout)
{
	struct tcp_ctx *tctx = ctx;
	int error, flags;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);
	PJDLOG_ASSERT(tctx->tc_side == TCP_SIDE_CLIENT);
	PJDLOG_ASSERT(tctx->tc_fd >= 0);
	PJDLOG_ASSERT(tctx->tc_sa.ss_family != AF_UNSPEC);
	PJDLOG_ASSERT(timeout >= -1);

	flags = fcntl(tctx->tc_fd, F_GETFL);
	if (flags == -1) {
		pjdlog_common(LOG_DEBUG, 1, errno, "fcntl(F_GETFL) failed");
		return (errno);
	}
	/*
	 * We make socket non-blocking so we can handle connection timeout
	 * manually.
	 */
	flags |= O_NONBLOCK;
	if (fcntl(tctx->tc_fd, F_SETFL, flags) == -1) {
		pjdlog_common(LOG_DEBUG, 1, errno,
		    "fcntl(F_SETFL, O_NONBLOCK) failed");
		return (errno);
	}

	if (connect(tctx->tc_fd, (struct sockaddr *)&tctx->tc_sa,
	    tctx->tc_sa.ss_len) == 0) {
		if (timeout == -1)
			return (0);
		error = 0;
		goto done;
	}
	if (errno != EINPROGRESS) {
		error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno, "connect() failed");
		goto done;
	}
	if (timeout == -1)
		return (0);
	return (tcp_connect_wait(ctx, timeout));
done:
	flags &= ~O_NONBLOCK;
	if (fcntl(tctx->tc_fd, F_SETFL, flags) == -1) {
		if (error == 0)
			error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno,
		    "fcntl(F_SETFL, ~O_NONBLOCK) failed");
	}
	return (error);
}

static int
tcp_connect_wait(void *ctx, int timeout)
{
	struct tcp_ctx *tctx = ctx;
	struct timeval tv;
	fd_set fdset;
	socklen_t esize;
	int error, flags, ret;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);
	PJDLOG_ASSERT(tctx->tc_side == TCP_SIDE_CLIENT);
	PJDLOG_ASSERT(tctx->tc_fd >= 0);
	PJDLOG_ASSERT(timeout >= 0);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
again:
	FD_ZERO(&fdset);
	FD_SET(tctx->tc_fd, &fdset);
	ret = select(tctx->tc_fd + 1, NULL, &fdset, NULL, &tv);
	if (ret == 0) {
		error = ETIMEDOUT;
		goto done;
	} else if (ret == -1) {
		if (errno == EINTR)
			goto again;
		error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno, "select() failed");
		goto done;
	}
	PJDLOG_ASSERT(ret > 0);
	PJDLOG_ASSERT(FD_ISSET(tctx->tc_fd, &fdset));
	esize = sizeof(error);
	if (getsockopt(tctx->tc_fd, SOL_SOCKET, SO_ERROR, &error,
	    &esize) == -1) {
		error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno,
		    "getsockopt(SO_ERROR) failed");
		goto done;
	}
	if (error != 0) {
		pjdlog_common(LOG_DEBUG, 1, error,
		    "getsockopt(SO_ERROR) returned error");
		goto done;
	}
	error = 0;
done:
	flags = fcntl(tctx->tc_fd, F_GETFL);
	if (flags == -1) {
		if (error == 0)
			error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno, "fcntl(F_GETFL) failed");
		return (error);
	}
	flags &= ~O_NONBLOCK;
	if (fcntl(tctx->tc_fd, F_SETFL, flags) == -1) {
		if (error == 0)
			error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno,
		    "fcntl(F_SETFL, ~O_NONBLOCK) failed");
	}
	return (error);
}

static int
tcp_server(const char *addr, void **ctxp)
{
	struct tcp_ctx *tctx;
	int ret, val;

	ret = tcp_setup_new(addr, TCP_SIDE_SERVER_LISTEN, ctxp);
	if (ret != 0)
		return (ret);

	tctx = *ctxp;

	val = 1;
	/* Ignore failure. */
	(void)setsockopt(tctx->tc_fd, SOL_SOCKET, SO_REUSEADDR, &val,
	   sizeof(val));

	PJDLOG_ASSERT(tctx->tc_sa.ss_family != AF_UNSPEC);

	if (bind(tctx->tc_fd, (struct sockaddr *)&tctx->tc_sa,
	    tctx->tc_sa.ss_len) == -1) {
		ret = errno;
		tcp_close(tctx);
		return (ret);
	}
	if (listen(tctx->tc_fd, 8) == -1) {
		ret = errno;
		tcp_close(tctx);
		return (ret);
	}

	return (0);
}

static int
tcp_accept(void *ctx, void **newctxp)
{
	struct tcp_ctx *tctx = ctx;
	struct tcp_ctx *newtctx;
	socklen_t fromlen;
	int ret;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);
	PJDLOG_ASSERT(tctx->tc_side == TCP_SIDE_SERVER_LISTEN);
	PJDLOG_ASSERT(tctx->tc_fd >= 0);
	PJDLOG_ASSERT(tctx->tc_sa.ss_family != AF_UNSPEC);

	newtctx = malloc(sizeof(*newtctx));
	if (newtctx == NULL)
		return (errno);

	fromlen = tctx->tc_sa.ss_len;
	newtctx->tc_fd = accept(tctx->tc_fd, (struct sockaddr *)&tctx->tc_sa,
	    &fromlen);
	if (newtctx->tc_fd == -1) {
		ret = errno;
		free(newtctx);
		return (ret);
	}

	newtctx->tc_side = TCP_SIDE_SERVER_WORK;
	newtctx->tc_magic = TCP_CTX_MAGIC;
	*newctxp = newtctx;

	return (0);
}

static int
tcp_wrap(int fd, bool client, void **ctxp)
{

	return (tcp_setup_wrap(fd,
	    client ? TCP_SIDE_CLIENT : TCP_SIDE_SERVER_WORK, ctxp));
}

static int
tcp_send(void *ctx, const unsigned char *data, size_t size, int fd)
{
	struct tcp_ctx *tctx = ctx;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);
	PJDLOG_ASSERT(tctx->tc_fd >= 0);
	PJDLOG_ASSERT(fd == -1);

	return (proto_common_send(tctx->tc_fd, data, size, -1));
}

static int
tcp_recv(void *ctx, unsigned char *data, size_t size, int *fdp)
{
	struct tcp_ctx *tctx = ctx;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);
	PJDLOG_ASSERT(tctx->tc_fd >= 0);
	PJDLOG_ASSERT(fdp == NULL);

	return (proto_common_recv(tctx->tc_fd, data, size, NULL));
}

static int
tcp_descriptor(const void *ctx)
{
	const struct tcp_ctx *tctx = ctx;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);

	return (tctx->tc_fd);
}

static bool
tcp_address_match(const void *ctx, const char *addr)
{
	const struct tcp_ctx *tctx = ctx;
	struct sockaddr_storage sa1, sa2;
	socklen_t salen;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);

	if (tcp_addr(addr, PROTO_TCP_DEFAULT_PORT, &sa1) != 0)
		return (false);

	salen = sizeof(sa2);
	if (getpeername(tctx->tc_fd, (struct sockaddr *)&sa2, &salen) == -1)
		return (false);

	if (sa1.ss_family != sa2.ss_family || sa1.ss_len != sa2.ss_len)
		return (false);

	switch (sa1.ss_family) {
	case AF_INET:
	    {
		struct sockaddr_in *sin1, *sin2;

		sin1 = (struct sockaddr_in *)&sa1;
		sin2 = (struct sockaddr_in *)&sa2;

		return (memcmp(&sin1->sin_addr, &sin2->sin_addr,
		    sizeof(sin1->sin_addr)) == 0);
	    }
	case AF_INET6:
	    {
		struct sockaddr_in6 *sin1, *sin2;

		sin1 = (struct sockaddr_in6 *)&sa1;
		sin2 = (struct sockaddr_in6 *)&sa2;

		return (memcmp(&sin1->sin6_addr, &sin2->sin6_addr,
		    sizeof(sin1->sin6_addr)) == 0);
	    }
	default:
		return (false);
	}
}

static void
tcp_local_address(const void *ctx, char *addr, size_t size)
{
	const struct tcp_ctx *tctx = ctx;
	struct sockaddr_storage sa;
	socklen_t salen;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);

	salen = sizeof(sa);
	if (getsockname(tctx->tc_fd, (struct sockaddr *)&sa, &salen) == -1) {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	PJDLOG_VERIFY(snprintf(addr, size, "tcp://%S", &sa) < (ssize_t)size);
}

static void
tcp_remote_address(const void *ctx, char *addr, size_t size)
{
	const struct tcp_ctx *tctx = ctx;
	struct sockaddr_storage sa;
	socklen_t salen;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);

	salen = sizeof(sa);
	if (getpeername(tctx->tc_fd, (struct sockaddr *)&sa, &salen) == -1) {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	PJDLOG_VERIFY(snprintf(addr, size, "tcp://%S", &sa) < (ssize_t)size);
}

static void
tcp_close(void *ctx)
{
	struct tcp_ctx *tctx = ctx;

	PJDLOG_ASSERT(tctx != NULL);
	PJDLOG_ASSERT(tctx->tc_magic == TCP_CTX_MAGIC);

	if (tctx->tc_fd >= 0)
		close(tctx->tc_fd);
	tctx->tc_magic = 0;
	free(tctx);
}

static struct proto tcp_proto = {
	.prt_name = "tcp",
	.prt_client = tcp_client,
	.prt_connect = tcp_connect,
	.prt_connect_wait = tcp_connect_wait,
	.prt_server = tcp_server,
	.prt_accept = tcp_accept,
	.prt_wrap = tcp_wrap,
	.prt_send = tcp_send,
	.prt_recv = tcp_recv,
	.prt_descriptor = tcp_descriptor,
	.prt_address_match = tcp_address_match,
	.prt_local_address = tcp_local_address,
	.prt_remote_address = tcp_remote_address,
	.prt_close = tcp_close
};

static __constructor void
tcp_ctor(void)
{

	proto_register(&tcp_proto, true);
}
