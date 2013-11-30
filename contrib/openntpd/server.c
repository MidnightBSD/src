/*	$OpenBSD: server.c,v 1.32 2008/06/10 03:46:09 naddy Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004 Alexander Guy <alexander@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"

int
setup_listeners(struct servent *se, struct ntpd_conf *lconf, u_int *cnt)
{
	struct listen_addr	*la;
	struct ifaddrs		*ifa, *ifap;
	struct sockaddr		*sa;
	u_int8_t		*a6;
	size_t			 sa6len = sizeof(struct in6_addr);
	u_int			 new_cnt = 0;
	int			 tos = IPTOS_LOWDELAY;

	if (lconf->listen_all) {
		if (getifaddrs(&ifa) == -1)
			fatal("getifaddrs");

		for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next) {
			sa = ifap->ifa_addr;

			if (sa == NULL ||
			    (sa->sa_family != AF_INET &&
			    sa->sa_family != AF_INET6))
				continue;
			if (SA_LEN(sa) == 0)
				continue;

			if (sa->sa_family == AF_INET &&
			    ((struct sockaddr_in *)sa)->sin_addr.s_addr ==
			    INADDR_ANY)
				continue;

			if (sa->sa_family == AF_INET6) {
				a6 = ((struct sockaddr_in6 *)sa)->
				    sin6_addr.s6_addr;
				if (memcmp(a6, &in6addr_any, sa6len) == 0)
					continue;
			}

			if ((la = calloc(1, sizeof(struct listen_addr))) ==
			    NULL)
				fatal("setup_listeners calloc");

			memcpy(&la->sa, sa, SA_LEN(sa));
			TAILQ_INSERT_TAIL(&lconf->listen_addrs, la, entry);
		}

		freeifaddrs(ifa);
	}

	TAILQ_FOREACH(la, &lconf->listen_addrs, entry) {
		new_cnt++;

		switch (la->sa.ss_family) {
		case AF_INET:
			if (((struct sockaddr_in *)&la->sa)->sin_port == 0)
				((struct sockaddr_in *)&la->sa)->sin_port =
				    se->s_port;
			break;
		case AF_INET6:
			if (((struct sockaddr_in6 *)&la->sa)->sin6_port == 0)
				((struct sockaddr_in6 *)&la->sa)->sin6_port =
				    se->s_port;
			break;
		default:
			fatalx("king bula sez: af borked");
		}

		log_info("listening on %s",
		    log_sockaddr((struct sockaddr *)&la->sa));

		if ((la->fd = socket(la->sa.ss_family, SOCK_DGRAM, 0)) == -1)
			fatal("socket");

		if (la->sa.ss_family == AF_INET && setsockopt(la->fd,
		    IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
			log_warn("setsockopt IPTOS_LOWDELAY");

		if (bind(la->fd, (struct sockaddr *)&la->sa,
		    SA_LEN((struct sockaddr *)&la->sa)) == -1)
			fatal("bind");
	}

	*cnt = new_cnt;

	return (0);
}

int
server_dispatch(int fd, struct ntpd_conf *lconf)
{
	ssize_t			 size;
	u_int8_t		 version;
	double			 rectime;
	struct sockaddr_storage	 fsa;
	socklen_t		 fsa_len;
	struct ntp_msg		 query, reply;
	char			 buf[NTP_MSGSIZE];

	fsa_len = sizeof(fsa);
	if ((size = recvfrom(fd, &buf, sizeof(buf), 0,
	    (struct sockaddr *)&fsa, &fsa_len)) == -1) {
		if (errno == EHOSTUNREACH || errno == EHOSTDOWN ||
		    errno == ENETUNREACH || errno == ENETDOWN) {
			log_warn("recvfrom %s",
			    log_sockaddr((struct sockaddr *)&fsa));
			return (0);
		} else
			fatal("recvfrom");
	}

	rectime = gettime_corrected();

	if (ntp_getmsg((struct sockaddr *)&fsa, buf, size, &query) == -1)
		return (0);

	version = (query.status & VERSIONMASK) >> 3;

	bzero(&reply, sizeof(reply));
	if (lconf->status.synced)
		reply.status = lconf->status.leap;
	else
		reply.status = LI_ALARM;
	reply.status |= (query.status & VERSIONMASK);
	if ((query.status & MODEMASK) == MODE_CLIENT)
		reply.status |= MODE_SERVER;
	else
		reply.status |= MODE_SYM_PAS;

	reply.stratum =	lconf->status.stratum;
	reply.ppoll = query.ppoll;
	reply.precision = lconf->status.precision;
	reply.rectime = d_to_lfp(rectime);
	reply.reftime = d_to_lfp(lconf->status.reftime);
	reply.xmttime = d_to_lfp(gettime_corrected());
	reply.orgtime = query.xmttime;
	reply.rootdelay = d_to_sfp(lconf->status.rootdelay);
	reply.refid = lconf->status.refid;

	ntp_sendmsg(fd, (struct sockaddr *)&fsa, &reply, size, 0);
	return (0);
}
