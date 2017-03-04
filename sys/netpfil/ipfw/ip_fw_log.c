/*-
 * Copyright (c) 2002-2009 Luigi Rizzo, Universita` di Pisa
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Logging support for ipfw
 */

#include "opt_ipfw.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/ethernet.h> /* for ETHERTYPE_IP */
#include <net/if.h>
#include <net/vnet.h>
#include <net/if_types.h>	/* for IFT_ETHER */
#include <net/bpf.h>		/* for BPF */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#ifdef INET6
#include <netinet6/in6_var.h>	/* ip6_sprintf() */
#endif

#include <netpfil/ipfw/ip_fw_private.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

/*
 * L3HDR maps an ipv4 pointer into a layer3 header pointer of type T
 * Other macros just cast void * into the appropriate type
 */
#define	L3HDR(T, ip)	((T *)((u_int32_t *)(ip) + (ip)->ip_hl))
#define	TCP(p)		((struct tcphdr *)(p))
#define	SCTP(p)		((struct sctphdr *)(p))
#define	UDP(p)		((struct udphdr *)(p))
#define	ICMP(p)		((struct icmphdr *)(p))
#define	ICMP6(p)	((struct icmp6_hdr *)(p))

#define SNPARGS(buf, len) buf + len, sizeof(buf) > len ? sizeof(buf) - len : 0
#define SNP(buf) buf, sizeof(buf)

#ifdef WITHOUT_BPF
void
ipfw_log_bpf(int onoff)
{
}
#else /* !WITHOUT_BPF */
static struct ifnet *log_if;	/* hook to attach to bpf */

/* we use this dummy function for all ifnet callbacks */
static int
log_dummy(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	return EINVAL;
}

static int
ipfw_log_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct route *ro)
{
	if (m != NULL)
		m_freem(m);
	return EINVAL;
}

static void
ipfw_log_start(struct ifnet* ifp)
{
	panic("ipfw_log_start() must not be called");
}

static const u_char ipfwbroadcastaddr[6] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

void
ipfw_log_bpf(int onoff)
{
	struct ifnet *ifp;

	if (onoff) {
		if (log_if)
			return;
		ifp = if_alloc(IFT_ETHER);
		if (ifp == NULL)
			return;
		if_initname(ifp, "ipfw", 0);
		ifp->if_mtu = 65536;
		ifp->if_flags = IFF_UP | IFF_SIMPLEX | IFF_MULTICAST;
		ifp->if_init = (void *)log_dummy;
		ifp->if_ioctl = log_dummy;
		ifp->if_start = ipfw_log_start;
		ifp->if_output = ipfw_log_output;
		ifp->if_addrlen = 6;
		ifp->if_hdrlen = 14;
		if_attach(ifp);
		ifp->if_broadcastaddr = ipfwbroadcastaddr;
		ifp->if_baudrate = IF_Mbps(10);
		bpfattach(ifp, DLT_EN10MB, 14);
		log_if = ifp;
	} else {
		if (log_if) {
			ether_ifdetach(log_if);
			if_free(log_if);
		}
		log_if = NULL;
	}
}
#endif /* !WITHOUT_BPF */

/*
 * We enter here when we have a rule with O_LOG.
 * XXX this function alone takes about 2Kbytes of code!
 */
void
ipfw_log(struct ip_fw *f, u_int hlen, struct ip_fw_args *args,
    struct mbuf *m, struct ifnet *oif, u_short offset, uint32_t tablearg,
    struct ip *ip)
{
	char *action;
	int limit_reached = 0;
	char action2[92], proto[128], fragment[32];

	if (V_fw_verbose == 0) {
#ifndef WITHOUT_BPF

		if (log_if == NULL || log_if->if_bpf == NULL)
			return;

		if (args->eh) /* layer2, use orig hdr */
			BPF_MTAP2(log_if, args->eh, ETHER_HDR_LEN, m);
		else
			/* Add fake header. Later we will store
			 * more info in the header.
			 */
			BPF_MTAP2(log_if, "DDDDDDSSSSSS\x08\x00", ETHER_HDR_LEN, m);
#endif /* !WITHOUT_BPF */
		return;
	}
	/* the old 'log' function */
	fragment[0] = '\0';
	proto[0] = '\0';

	if (f == NULL) {	/* bogus pkt */
		if (V_verbose_limit != 0 && V_norule_counter >= V_verbose_limit)
			return;
		V_norule_counter++;
		if (V_norule_counter == V_verbose_limit)
			limit_reached = V_verbose_limit;
		action = "Refuse";
	} else {	/* O_LOG is the first action, find the real one */
		ipfw_insn *cmd = ACTION_PTR(f);
		ipfw_insn_log *l = (ipfw_insn_log *)cmd;

		if (l->max_log != 0 && l->log_left == 0)
			return;
		l->log_left--;
		if (l->log_left == 0)
			limit_reached = l->max_log;
		cmd += F_LEN(cmd);	/* point to first action */
		if (cmd->opcode == O_ALTQ) {
			ipfw_insn_altq *altq = (ipfw_insn_altq *)cmd;

			snprintf(SNPARGS(action2, 0), "Altq %d",
				altq->qid);
			cmd += F_LEN(cmd);
		}
		if (cmd->opcode == O_PROB || cmd->opcode == O_TAG ||
		    cmd->opcode == O_SETDSCP)
			cmd += F_LEN(cmd);

		action = action2;
		switch (cmd->opcode) {
		case O_DENY:
			action = "Deny";
			break;

		case O_REJECT:
			if (cmd->arg1==ICMP_REJECT_RST)
				action = "Reset";
			else if (cmd->arg1==ICMP_UNREACH_HOST)
				action = "Reject";
			else
				snprintf(SNPARGS(action2, 0), "Unreach %d",
					cmd->arg1);
			break;

		case O_UNREACH6:
			if (cmd->arg1==ICMP6_UNREACH_RST)
				action = "Reset";
			else
				snprintf(SNPARGS(action2, 0), "Unreach %d",
					cmd->arg1);
			break;

		case O_ACCEPT:
			action = "Accept";
			break;
		case O_COUNT:
			action = "Count";
			break;
		case O_DIVERT:
			snprintf(SNPARGS(action2, 0), "Divert %d",
				cmd->arg1);
			break;
		case O_TEE:
			snprintf(SNPARGS(action2, 0), "Tee %d",
				cmd->arg1);
			break;
		case O_SETFIB:
			snprintf(SNPARGS(action2, 0), "SetFib %d",
				IP_FW_ARG_TABLEARG(cmd->arg1));
			break;
		case O_SKIPTO:
			snprintf(SNPARGS(action2, 0), "SkipTo %d",
				IP_FW_ARG_TABLEARG(cmd->arg1));
			break;
		case O_PIPE:
			snprintf(SNPARGS(action2, 0), "Pipe %d",
				IP_FW_ARG_TABLEARG(cmd->arg1));
			break;
		case O_QUEUE:
			snprintf(SNPARGS(action2, 0), "Queue %d",
				IP_FW_ARG_TABLEARG(cmd->arg1));
			break;
		case O_FORWARD_IP: {
			ipfw_insn_sa *sa = (ipfw_insn_sa *)cmd;
			int len;
			struct in_addr dummyaddr;
			if (sa->sa.sin_addr.s_addr == INADDR_ANY)
				dummyaddr.s_addr = htonl(tablearg);
			else
				dummyaddr.s_addr = sa->sa.sin_addr.s_addr;

			len = snprintf(SNPARGS(action2, 0), "Forward to %s",
				inet_ntoa(dummyaddr));

			if (sa->sa.sin_port)
				snprintf(SNPARGS(action2, len), ":%d",
				    sa->sa.sin_port);
			}
			break;
#ifdef INET6
		case O_FORWARD_IP6: {
			char buf[INET6_ADDRSTRLEN];
			ipfw_insn_sa6 *sa = (ipfw_insn_sa6 *)cmd;
			int len;

			len = snprintf(SNPARGS(action2, 0), "Forward to [%s]",
			    ip6_sprintf(buf, &sa->sa.sin6_addr));

			if (sa->sa.sin6_port)
				snprintf(SNPARGS(action2, len), ":%u",
				    sa->sa.sin6_port);
			}
			break;
#endif
		case O_NETGRAPH:
			snprintf(SNPARGS(action2, 0), "Netgraph %d",
				cmd->arg1);
			break;
		case O_NGTEE:
			snprintf(SNPARGS(action2, 0), "Ngtee %d",
				cmd->arg1);
			break;
		case O_NAT:
			action = "Nat";
 			break;
		case O_REASS:
			action = "Reass";
			break;
		case O_CALLRETURN:
			if (cmd->len & F_NOT)
				action = "Return";
			else
				snprintf(SNPARGS(action2, 0), "Call %d",
				    cmd->arg1);
			break;
		default:
			action = "UNKNOWN";
			break;
		}
	}

	if (hlen == 0) {	/* non-ip */
		snprintf(SNPARGS(proto, 0), "MAC");

	} else {
		int len;
#ifdef INET6
		char src[INET6_ADDRSTRLEN + 2], dst[INET6_ADDRSTRLEN + 2];
#else
		char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
#endif
		struct icmphdr *icmp;
		struct tcphdr *tcp;
		struct udphdr *udp;
#ifdef INET6
		struct ip6_hdr *ip6 = NULL;
		struct icmp6_hdr *icmp6;
		u_short ip6f_mf;
#endif
		src[0] = '\0';
		dst[0] = '\0';
#ifdef INET6
		ip6f_mf = offset & IP6F_MORE_FRAG;
		offset &= IP6F_OFF_MASK;

		if (IS_IP6_FLOW_ID(&(args->f_id))) {
			char ip6buf[INET6_ADDRSTRLEN];
			snprintf(src, sizeof(src), "[%s]",
			    ip6_sprintf(ip6buf, &args->f_id.src_ip6));
			snprintf(dst, sizeof(dst), "[%s]",
			    ip6_sprintf(ip6buf, &args->f_id.dst_ip6));

			ip6 = (struct ip6_hdr *)ip;
			tcp = (struct tcphdr *)(((char *)ip) + hlen);
			udp = (struct udphdr *)(((char *)ip) + hlen);
		} else
#endif
		{
			tcp = L3HDR(struct tcphdr, ip);
			udp = L3HDR(struct udphdr, ip);

			inet_ntop(AF_INET, &ip->ip_src, src, sizeof(src));
			inet_ntop(AF_INET, &ip->ip_dst, dst, sizeof(dst));
		}

		switch (args->f_id.proto) {
		case IPPROTO_TCP:
			len = snprintf(SNPARGS(proto, 0), "TCP %s", src);
			if (offset == 0)
				snprintf(SNPARGS(proto, len), ":%d %s:%d",
				    ntohs(tcp->th_sport),
				    dst,
				    ntohs(tcp->th_dport));
			else
				snprintf(SNPARGS(proto, len), " %s", dst);
			break;

		case IPPROTO_UDP:
			len = snprintf(SNPARGS(proto, 0), "UDP %s", src);
			if (offset == 0)
				snprintf(SNPARGS(proto, len), ":%d %s:%d",
				    ntohs(udp->uh_sport),
				    dst,
				    ntohs(udp->uh_dport));
			else
				snprintf(SNPARGS(proto, len), " %s", dst);
			break;

		case IPPROTO_ICMP:
			icmp = L3HDR(struct icmphdr, ip);
			if (offset == 0)
				len = snprintf(SNPARGS(proto, 0),
				    "ICMP:%u.%u ",
				    icmp->icmp_type, icmp->icmp_code);
			else
				len = snprintf(SNPARGS(proto, 0), "ICMP ");
			len += snprintf(SNPARGS(proto, len), "%s", src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
#ifdef INET6
		case IPPROTO_ICMPV6:
			icmp6 = (struct icmp6_hdr *)(((char *)ip) + hlen);
			if (offset == 0)
				len = snprintf(SNPARGS(proto, 0),
				    "ICMPv6:%u.%u ",
				    icmp6->icmp6_type, icmp6->icmp6_code);
			else
				len = snprintf(SNPARGS(proto, 0), "ICMPv6 ");
			len += snprintf(SNPARGS(proto, len), "%s", src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
#endif
		default:
			len = snprintf(SNPARGS(proto, 0), "P:%d %s",
			    args->f_id.proto, src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
		}

#ifdef INET6
		if (IS_IP6_FLOW_ID(&(args->f_id))) {
			if (offset & (IP6F_OFF_MASK | IP6F_MORE_FRAG))
				snprintf(SNPARGS(fragment, 0),
				    " (frag %08x:%d@%d%s)",
				    args->f_id.extra,
				    ntohs(ip6->ip6_plen) - hlen,
				    ntohs(offset) << 3, ip6f_mf ? "+" : "");
		} else
#endif
		{
			int ipoff, iplen;
			ipoff = ntohs(ip->ip_off);
			iplen = ntohs(ip->ip_len);
			if (ipoff & (IP_MF | IP_OFFMASK))
				snprintf(SNPARGS(fragment, 0),
				    " (frag %d:%d@%d%s)",
				    ntohs(ip->ip_id), iplen - (ip->ip_hl << 2),
				    offset << 3,
				    (ipoff & IP_MF) ? "+" : "");
		}
	}
#ifdef __FreeBSD__
	if (oif || m->m_pkthdr.rcvif)
		log(LOG_SECURITY | LOG_INFO,
		    "ipfw: %d %s %s %s via %s%s\n",
		    f ? f->rulenum : -1,
		    action, proto, oif ? "out" : "in",
		    oif ? oif->if_xname : m->m_pkthdr.rcvif->if_xname,
		    fragment);
	else
#endif
		log(LOG_SECURITY | LOG_INFO,
		    "ipfw: %d %s %s [no if info]%s\n",
		    f ? f->rulenum : -1,
		    action, proto, fragment);
	if (limit_reached)
		log(LOG_SECURITY | LOG_NOTICE,
		    "ipfw: limit %d reached on entry %d\n",
		    limit_reached, f ? f->rulenum : -1);
}
/* end of file */
