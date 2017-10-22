/*-
 * Copyright (c) 2013 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/10.0.0/sys/netinet/in_kdtrace.c 255993 2013-10-02 17:14:12Z markj $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/netinet/in_kdtrace.c 255993 2013-10-02 17:14:12Z markj $");

#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sdt.h>

SDT_PROVIDER_DEFINE(ip);
SDT_PROVIDER_DEFINE(tcp);
SDT_PROVIDER_DEFINE(udp);

SDT_PROBE_DEFINE6_XLATE(ip, , , receive, receive,
    "void *", "pktinfo_t *",
    "void *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct ifnet *", "ifinfo_t *",
    "struct ip *", "ipv4info_t *",
    "struct ip6_hdr *", "ipv6info_t *");

SDT_PROBE_DEFINE6_XLATE(ip, , , send, send,
    "void *", "pktinfo_t *",
    "void *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct ifnet *", "ifinfo_t *",
    "struct ip *", "ipv4info_t *",
    "struct ip6_hdr *", "ipv6info_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , accept_established, accept-established,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , accept_refused, accept-refused,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , connect_established, connect-established,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , connect_refused, connect-refused,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , connect_request, connect-request,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , receive, receive,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , send, send,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *");

SDT_PROBE_DEFINE6_XLATE(tcp, , , state_change, state-change,
    "void *", "void *",
    "struct tcpcb *", "csinfo_t *",
    "void *", "void *",
    "struct tcpcb *", "tcpsinfo_t *",
    "void *", "void *",
    "int", "tcplsinfo_t *");

SDT_PROBE_DEFINE5_XLATE(udp, , , receive, receive,
    "void *", "pktinfo_t *",
    "struct inpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct inpcb *", "udpsinfo_t *",
    "struct udphdr *", "udpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(udp, , , send, send,
    "void *", "pktinfo_t *",
    "struct inpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct inpcb *", "udpsinfo_t *",
    "struct udphdr *", "udpinfo_t *");
