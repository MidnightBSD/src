/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $FreeBSD: src/usr.sbin/mrouted/prune.h,v 1.10 1999/08/28 01:17:08 peter Exp $
 * prune.h,v 3.8.4.5 1998/02/27 22:45:43 fenner Exp
 */

/*
 * Group table
 *
 * Each group entry is a member of two doubly-linked lists:
 *
 * a) A list hanging off of the routing table entry for this source (rt_groups)
 *	sorted by group address under the routing entry (gt_next, gt_prev)
 * b) An independent list pointed to by kernel_table, which is a list of
 *	active source,group's (gt_gnext, gt_gprev).
 *
 */
struct gtable {
    struct gtable  *gt_next;		/* pointer to the next entry	    */
    struct gtable  *gt_prev;		/* back pointer for linked list	    */
    struct gtable  *gt_gnext;		/* fwd pointer for group list	    */
    struct gtable  *gt_gprev;		/* rev pointer for group list	    */
    u_int32	    gt_mcastgrp;	/* multicast group associated       */
    vifbitmap_t     gt_scope;		/* scoped interfaces                */
    u_char	    gt_ttls[MAXVIFS];	/* ttl vector for forwarding        */
    vifbitmap_t	    gt_grpmems;		/* forw. vifs for src, grp          */
    int		    gt_prsent_timer;	/* prune timer for this group	    */
    int  	    gt_timer;		/* timer for this group entry	    */
    time_t 	    gt_ctime;		/* time of entry creation	    */
    u_char	    gt_grftsnt;		/* graft sent/retransmit timer	    */
    nbrbitmap_t	    gt_prunes;		/* bitmap of neighbors who pruned   */
    struct stable  *gt_srctbl;		/* source table			    */
    struct ptable  *gt_pruntbl;		/* prune table			    */
    struct rtentry *gt_route;		/* parent route			    */
    int		    gt_rexmit_timer;	/* timer for prune retransmission   */
    int		    gt_prune_rexmit;	/* time til prune retransmission    */
#ifdef RSRR
    struct rsrr_cache *gt_rsrr_cache;	/* RSRR cache                       */
#endif /* RSRR */
};

/*
 * Source table
 *
 * When source-based prunes exist, there will be a struct ptable here as well.
 */
struct stable 
{
    struct stable  *st_next;       	/* pointer to the next entry        */
    u_int32	    st_origin;		/* host origin of multicasts        */
    u_long	    st_pktcnt;		/* packet count for src-grp entry   */
    u_long	    st_savpkt;		/* saved pkt cnt when no krnl entry */
    time_t	    st_ctime;		/* kernel entry creation time	    */
};

/*
 * structure to store incoming prunes.  Can hang off of either group or source.
 */
struct ptable 
{
    struct ptable  *pt_next;		/* pointer to the next entry	    */
    u_int32	    pt_router;		/* router that sent this prune	    */
    vifi_t	    pt_vifi;		/* vif prune received on	    */
    int		    pt_index;		/* neighbor index of router	    */
    int		    pt_timer;		/* timer for prune		    */
};

#define MIN_PRUNE_LIFE	TIMER_INTERVAL	/* min prune lifetime to bother with */

/*
 * The packet format for a traceroute request.
 */
struct tr_query {
    u_int32  tr_src;		/* traceroute source */
    u_int32  tr_dst;		/* traceroute destination */
    u_int32  tr_raddr;		/* traceroute response address */
#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
    struct {
	u_int	qid : 24;	/* traceroute query id */
	u_int	ttl : 8;	/* traceroute response ttl */
    } q;
#else
    struct {
	u_int   ttl : 8;	/* traceroute response ttl */
	u_int   qid : 24;	/* traceroute query id */
    } q;
#endif /* BYTE_ORDER */
};

#define tr_rttl q.ttl
#define tr_qid  q.qid

/*
 * Traceroute response format.  A traceroute response has a tr_query at the
 * beginning, followed by one tr_resp for each hop taken.
 */
struct tr_resp {
    u_int32 tr_qarr;		/* query arrival time */
    u_int32 tr_inaddr;		/* incoming interface address */
    u_int32 tr_outaddr;		/* outgoing interface address */
    u_int32 tr_rmtaddr;		/* parent address in source tree */
    u_int32 tr_vifin;		/* input packet count on interface */
    u_int32 tr_vifout;		/* output packet count on interface */
    u_int32 tr_pktcnt;		/* total incoming packets for src-grp */
    u_char  tr_rproto;		/* routing protocol deployed on router */
    u_char  tr_fttl;		/* ttl required to forward on outvif */
    u_char  tr_smask;		/* subnet mask for src addr */
    u_char  tr_rflags;		/* forwarding error codes */
};

/* defs within mtrace */
#define QUERY	1
#define RESP	2
#define QLEN	sizeof(struct tr_query)
#define RLEN	sizeof(struct tr_resp)

/* fields for tr_rflags (forwarding error codes) */
#define TR_NO_ERR	0
#define TR_WRONG_IF	1
#define TR_PRUNED	2
#define TR_OPRUNED	3
#define TR_SCOPED	4
#define TR_NO_RTE	5
#define TR_NO_FWD	7
#define TR_NO_SPACE	0x81
#define TR_OLD_ROUTER	0x82

/* fields for tr_rproto (routing protocol) */
#define PROTO_DVMRP	1
#define PROTO_MOSPF	2
#define PROTO_PIM	3
#define PROTO_CBT	4

#define MASK_TO_VAL(x, i) { \
			u_int32 _x = ntohl(x); \
			(i) = 1; \
			while ((_x) <<= 1) \
				(i)++; \
			};

#define VAL_TO_MASK(x, i) { \
			x = i ? htonl(~((1 << (32 - (i))) - 1)) : 0; \
			};

#define NBR_VERS(n)	(((n)->al_pv << 8) + (n)->al_mv)
