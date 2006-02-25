/*
 * NETBIOS protocol formats
 *
 * @(#) $Header: /home/cvs/src/contrib/tcpdump/netbios.h,v 1.1.1.1 2006-02-25 02:26:23 laffer1 Exp $
 */

struct p8022Hdr {
    u_char	dsap;
    u_char	ssap;
    u_char	flags;
};

#define	p8022Size	3		/* min 802.2 header size */

#define UI		0x03		/* 802.2 flags */

