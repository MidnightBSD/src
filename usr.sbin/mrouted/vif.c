/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * vif.c,v 3.8.4.56.2.1 1999/01/20 05:18:50 fenner Exp
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/mrouted/vif.c,v 1.15 1999/08/28 01:17:09 peter Exp $";
#endif /* not lint */

#include "defs.h"
#include <fcntl.h>

/*
 * Exported variables.
 */
struct uvif	uvifs[MAXVIFS];	/* array of virtual interfaces		    */
vifi_t		numvifs;	/* number of vifs in use	    	    */
int		vifs_down;	/* 1=>some interfaces are down	    	    */
int		phys_vif;	/* An enabled vif		    	    */
int		udp_socket;	/* Since the honkin' kernel doesn't support */
				/* ioctls on raw IP sockets, we need a UDP  */
				/* socket as well as our IGMP (raw) socket. */
				/* How dumb.                                */
int		vifs_with_neighbors;	/* == 1 if I am a leaf		    */

/*
 * Private variables.
 */
struct listaddr	*nbrs[MAXNBRS];	/* array of neighbors			    */

typedef struct {
        vifi_t  vifi;
        struct listaddr *g;
	int    q_time;
} cbk_t;

/*
 * Forward declarations.
 */
static void start_vif __P((vifi_t vifi));
static void start_vif2 __P((vifi_t vifi));
static void stop_vif __P((vifi_t vifi));
static void age_old_hosts __P((void));
static void send_probe_on_vif __P((struct uvif *v));
static void send_query __P((struct uvif *v));
static int info_version __P((char *p, int plen));
static void DelVif __P((void *arg));
static int SetTimer __P((int vifi, struct listaddr *g));
static int DeleteTimer __P((int id));
static void SendQuery __P((void *arg));
static int SetQueryTimer __P((struct listaddr *g, vifi_t vifi, int to_expire,
					int q_time));


/*
 * Initialize the virtual interfaces, but do not install
 * them in the kernel.  Start routing on all vifs that are
 * not down or disabled.
 */
void
init_vifs()
{
    vifi_t vifi;
    struct uvif *v;
    int enabled_vifs, enabled_phyints;
    extern char *configfilename;

    numvifs = 0;
    vifs_with_neighbors = 0;
    vifs_down = FALSE;

    /*
     * Configure the vifs based on the interface configuration of the
     * the kernel and the contents of the configuration file.
     * (Open a UDP socket for ioctl use in the config procedures if
     * the kernel can't handle IOCTL's on the IGMP socket.)
     */
#ifdef IOCTL_OK_ON_RAW_SOCKET
    udp_socket = igmp_socket;
#else
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	log(LOG_ERR, errno, "UDP socket");
#endif
    log(LOG_INFO,0,"Getting vifs from kernel interfaces");
    config_vifs_from_kernel();
    log(LOG_INFO,0,"Getting vifs from %s",configfilename);
    config_vifs_from_file();

    /*
     * Quit if there are fewer than two enabled vifs.
     */
    enabled_vifs    = 0;
    enabled_phyints = 0;
    phys_vif	    = -1;
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!(v->uv_flags & VIFF_DISABLED)) {
	    ++enabled_vifs;
	    if (!(v->uv_flags & VIFF_TUNNEL)) {
    	    	if (phys_vif == -1)
    	    	    phys_vif = vifi;
		++enabled_phyints;
	    }
	}
    }
    if (enabled_vifs < 2)
	log(LOG_ERR, 0, "can't forward: %s",
	    enabled_vifs == 0 ? "no enabled vifs" : "only one enabled vif");

    if (enabled_phyints == 0)
	log(LOG_WARNING, 0,
	    "no enabled interfaces, forwarding via tunnels only");

    log(LOG_INFO, 0, "Installing vifs in mrouted...");
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!(v->uv_flags & VIFF_DISABLED)) {
	    if (!(v->uv_flags & VIFF_DOWN)) {
		if (v->uv_flags & VIFF_TUNNEL)
		    log(LOG_INFO, 0, "vif #%d, tunnel %s -> %s", vifi,
				inet_fmt(v->uv_lcl_addr, s1),
				inet_fmt(v->uv_rmt_addr, s2));
		else
		    log(LOG_INFO, 0, "vif #%d, phyint %s", vifi,
				inet_fmt(v->uv_lcl_addr, s1));
		start_vif2(vifi);
	    } else log(LOG_INFO, 0,
		     "%s is not yet up; vif #%u not in service",
		     v->uv_name, vifi);
	}
    }
}

/*
 * Initialize the passed vif with all appropriate default values.
 * "t" is true if a tunnel, or false if a phyint.
 */
void
zero_vif(v, t)
    struct uvif *v;
    int t;
{
    v->uv_flags		= 0;
    v->uv_metric	= DEFAULT_METRIC;
    v->uv_admetric	= 0;
    v->uv_threshold	= DEFAULT_THRESHOLD;
    v->uv_rate_limit	= t ? DEFAULT_TUN_RATE_LIMIT : DEFAULT_PHY_RATE_LIMIT;
    v->uv_lcl_addr	= 0;
    v->uv_rmt_addr	= 0;
    v->uv_dst_addr	= t ? 0 : dvmrp_group;
    v->uv_subnet	= 0;
    v->uv_subnetmask	= 0;
    v->uv_subnetbcast	= 0;
    v->uv_name[0]	= '\0';
    v->uv_groups	= NULL;
    v->uv_neighbors	= NULL;
    NBRM_CLRALL(v->uv_nbrmap);
    v->uv_querier	= NULL;
    v->uv_igmpv1_warn	= 0;
    v->uv_prune_lifetime = 0;
    v->uv_leaf_timer	= 0;
    v->uv_acl		= NULL;
    v->uv_addrs		= NULL;
    v->uv_filter	= NULL;
    v->uv_blasterbuf	= NULL;
    v->uv_blastercur	= NULL;
    v->uv_blasterend	= NULL;
    v->uv_blasterlen	= 0;
    v->uv_blastertimer	= 0;
    v->uv_nbrup		= 0;
    v->uv_icmp_warn	= 0;
    v->uv_nroutes	= 0;
}

/*
 * Start routing on all virtual interfaces that are not down or
 * administratively disabled.
 */
void
init_installvifs()
{
    vifi_t vifi;
    struct uvif *v;

    log(LOG_INFO, 0, "Installing vifs in kernel...");
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!(v->uv_flags & VIFF_DISABLED)) {
	    if (!(v->uv_flags & VIFF_DOWN)) {
		if (v->uv_flags & VIFF_TUNNEL)
		    log(LOG_INFO, 0, "vif #%d, tunnel %s -> %s", vifi,
				inet_fmt(v->uv_lcl_addr, s1),
				inet_fmt(v->uv_rmt_addr, s2));
		else
		    log(LOG_INFO, 0, "vif #%d, phyint %s", vifi,
				inet_fmt(v->uv_lcl_addr, s1));
		k_add_vif(vifi, &uvifs[vifi]);
	    } else log(LOG_INFO, 0,
		     "%s is not yet up; vif #%u not in service",
		     v->uv_name, vifi);
	}
    }
}

/*
 * See if any interfaces have changed from up state to down, or vice versa,
 * including any non-multicast-capable interfaces that are in use as local
 * tunnel end-points.  Ignore interfaces that have been administratively
 * disabled.
 */
void
check_vif_state()
{
    register vifi_t vifi;
    register struct uvif *v;
    struct ifreq ifr;
    static int checking_vifs = 0;

    /*
     * If we get an error while checking, (e.g. two interfaces go down
     * at once, and we decide to send a prune out one of the failed ones)
     * then don't go into an infinite loop!
     */
    if (checking_vifs)
	return;

    vifs_down = FALSE;
    checking_vifs = 1;
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {

	if (v->uv_flags & VIFF_DISABLED) continue;

	strncpy(ifr.ifr_name, v->uv_name, IFNAMSIZ);
	if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ifr) < 0)
	    log(LOG_ERR, errno,
		"ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);

	if (v->uv_flags & VIFF_DOWN) {
	    if (ifr.ifr_flags & IFF_UP) {
		log(LOG_NOTICE, 0,
		    "%s has come up; vif #%u now in service",
		    v->uv_name, vifi);
		v->uv_flags &= ~VIFF_DOWN;
		start_vif(vifi);
	    }
	    else vifs_down = TRUE;
	}
	else {
	    if (!(ifr.ifr_flags & IFF_UP)) {
		log(LOG_NOTICE, 0,
		    "%s has gone down; vif #%u taken out of service",
		    v->uv_name, vifi);
		stop_vif(vifi);
		v->uv_flags |= VIFF_DOWN;
		vifs_down = TRUE;
	    }
	}
    }
    checking_vifs = 0;
}

/*
 * Send a DVMRP message on the specified vif.  If DVMRP messages are
 * to be encapsulated and sent "inside" the tunnel, use the special
 * encapsulator.  If it's not a tunnel or DVMRP messages are to be
 * sent "beside" the tunnel, as required by earlier versions of mrouted,
 * then just send the message.
 */
void
send_on_vif(v, dst, code, datalen)
    register struct uvif *v;
    u_int32 dst;
    int code;
    int datalen;
{
    u_int32 group = htonl(MROUTED_LEVEL | 
			((v->uv_flags & VIFF_LEAF) ? 0 : LEAF_FLAGS));

    /*
     * The UNIX kernel will not decapsulate unicasts.
     * Therefore, we don't send encapsulated unicasts.
     */
    if ((v->uv_flags & (VIFF_TUNNEL|VIFF_OTUNNEL)) == VIFF_TUNNEL &&
	((dst == 0) || IN_MULTICAST(ntohl(dst))))
	send_ipip(v->uv_lcl_addr, dst ? dst : dvmrp_group, IGMP_DVMRP,
						code, group, datalen, v);
    else
	send_igmp(v->uv_lcl_addr, dst ? dst : v->uv_dst_addr, IGMP_DVMRP,
						code, group, datalen);
}


/*
 * Send a probe message on vif v
 */
static void
send_probe_on_vif(v)
    register struct uvif *v;
{
    register char *p;
    register int datalen = 0;
    struct listaddr *nbr;
    int i;

    if ((v->uv_flags & VIFF_PASSIVE && v->uv_neighbors == NULL) ||
	(v->uv_flags & VIFF_FORCE_LEAF))
	return;

    p = send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN;

    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(dvmrp_genid))[i];
    datalen += 4;

    /*
     * add the neighbor list on the interface to the message
     */
    nbr = v->uv_neighbors;

    while (nbr) {
	for (i = 0; i < 4; i++)
	    *p++ = ((char *)&nbr->al_addr)[i];
	datalen +=4;
	nbr = nbr->al_next;
    }

    send_on_vif(v, 0, DVMRP_PROBE, datalen);
}

static void
send_query(v)
    register struct uvif *v;
{
    IF_DEBUG(DEBUG_IGMP)
    log(LOG_DEBUG, 0, "sending %squery on vif %d",
		(v->uv_flags & VIFF_IGMPV1) ? "v1 " : "",
		v - uvifs);
    send_igmp(v->uv_lcl_addr, allhosts_group,
		IGMP_MEMBERSHIP_QUERY, 
		(v->uv_flags & VIFF_IGMPV1) ? 0 :
		IGMP_MAX_HOST_REPORT_DELAY * IGMP_TIMER_SCALE, 0, 0);
}

/*
 * Add a vifi to the kernel and start routing on it.
 */
static void
start_vif(vifi)
    vifi_t vifi;
{
    /*
     * Install the interface in the kernel's vif structure.
     */
    k_add_vif(vifi, &uvifs[vifi]);

    start_vif2(vifi);
}

/*
 * Add a vifi to all the user-level data structures but don't add
 * it to the kernel yet.
 */
static void
start_vif2(vifi)
    vifi_t vifi;
{
    struct uvif *v;
    u_int32 src;
    struct phaddr *p;

    v   = &uvifs[vifi];
    src = v->uv_lcl_addr;

    /*
     * Update the existing route entries to take into account the new vif.
     */
    add_vif_to_routes(vifi);

    if (!(v->uv_flags & VIFF_TUNNEL)) {
	/*
	 * Join the DVMRP multicast group on the interface.
	 * (This is not strictly necessary, since the kernel promiscuously
	 * receives IGMP packets addressed to ANY IP multicast group while
	 * multicast routing is enabled.  However, joining the group allows
	 * this host to receive non-IGMP packets as well, such as 'pings'.)
	 */
	k_join(dvmrp_group, src);

	/*
	 * Join the ALL-ROUTERS multicast group on the interface.
	 * This allows mtrace requests to loop back if they are run
	 * on the multicast router.
	 */
	k_join(allrtrs_group, src);

	/*
	 * Install an entry in the routing table for the subnet to which
	 * the interface is connected.
	 */
	start_route_updates();
	update_route(v->uv_subnet, v->uv_subnetmask, 0, 0, vifi, NULL);
	for (p = v->uv_addrs; p; p = p->pa_next) {
	    start_route_updates();
	    update_route(p->pa_subnet, p->pa_subnetmask, 0, 0, vifi, NULL);
	}

	/*
	 * Until neighbors are discovered, assume responsibility for sending
	 * periodic group membership queries to the subnet.  Send the first
	 * query.
	 */
	v->uv_flags |= VIFF_QUERIER;
	IF_DEBUG(DEBUG_IGMP)
	log(LOG_DEBUG, 0, "assuming querier duties on vif %d", vifi);
	send_query(v);
    }

    v->uv_leaf_timer = LEAF_CONFIRMATION_TIME;

    /*
     * Send a probe via the new vif to look for neighbors.
     */
    send_probe_on_vif(v);
}

/*
 * Stop routing on the specified virtual interface.
 */
static void
stop_vif(vifi)
    vifi_t vifi;
{
    struct uvif *v;
    struct listaddr *a;
    struct phaddr *p;

    v = &uvifs[vifi];

    if (!(v->uv_flags & VIFF_TUNNEL)) {
	/*
	 * Depart from the DVMRP multicast group on the interface.
	 */
	k_leave(dvmrp_group, v->uv_lcl_addr);

	/*
	 * Depart from the ALL-ROUTERS multicast group on the interface.
	 */
	k_leave(allrtrs_group, v->uv_lcl_addr);

	/*
	 * Update the entry in the routing table for the subnet to which
	 * the interface is connected, to take into account the interface
	 * failure.
	 */
	start_route_updates();
	update_route(v->uv_subnet, v->uv_subnetmask, UNREACHABLE, 0, vifi, NULL);
	for (p = v->uv_addrs; p; p = p->pa_next) {
	    start_route_updates();
	    update_route(p->pa_subnet, p->pa_subnetmask, UNREACHABLE, 0, vifi, NULL);
	}

	/*
	 * Discard all group addresses.  (No need to tell kernel;
	 * the k_del_vif() call, below, will clean up kernel state.)
	 */
	while (v->uv_groups != NULL) {
	    a = v->uv_groups;
	    v->uv_groups = a->al_next;
	    free((char *)a);
	}

	IF_DEBUG(DEBUG_IGMP)
	log(LOG_DEBUG, 0, "releasing querier duties on vif %d", vifi);
	v->uv_flags &= ~VIFF_QUERIER;
    }

    /*
     * Update the existing route entries to take into account the vif failure.
     */
    delete_vif_from_routes(vifi);

    /*
     * Delete the interface from the kernel's vif structure.
     */
    k_del_vif(vifi);

    /*
     * Discard all neighbor addresses.
     */
    if (!NBRM_ISEMPTY(v->uv_nbrmap))
	vifs_with_neighbors--;

    while (v->uv_neighbors != NULL) {
	a = v->uv_neighbors;
	v->uv_neighbors = a->al_next;
	nbrs[a->al_index] = NULL;
	free((char *)a);
    }
    NBRM_CLRALL(v->uv_nbrmap);
}


/*
 * stop routing on all vifs
 */
void
stop_all_vifs()
{
    vifi_t vifi;
    struct uvif *v;
    struct listaddr *a;
    struct vif_acl *acl;

    for (vifi = 0; vifi < numvifs; vifi++) {
	v = &uvifs[vifi];
	while (v->uv_groups != NULL) {
	    a = v->uv_groups;
	    v->uv_groups = a->al_next;
	    free((char *)a);
	}
	while (v->uv_neighbors != NULL) {
	    a = v->uv_neighbors;
	    v->uv_neighbors = a->al_next;
	    nbrs[a->al_index] = NULL;
	    free((char *)a);
	}
	while (v->uv_acl != NULL) {
	    acl = v->uv_acl;
	    v->uv_acl = acl->acl_next;
	    free((char *)acl);
	}
    }
}


/*
 * Find the virtual interface from which an incoming packet arrived,
 * based on the packet's source and destination IP addresses.
 */
vifi_t
find_vif(src, dst)
    register u_int32 src;
    register u_int32 dst;
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct phaddr *p;

    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!(v->uv_flags & (VIFF_DOWN|VIFF_DISABLED))) {
	    if (v->uv_flags & VIFF_TUNNEL) {
		if (src == v->uv_rmt_addr && (dst == v->uv_lcl_addr ||
					      dst == dvmrp_group))
		    return(vifi);
	    }
	    else {
		if ((src & v->uv_subnetmask) == v->uv_subnet &&
		    ((v->uv_subnetmask == 0xffffffff) ||
		     (src != v->uv_subnetbcast)))
		    return(vifi);
		for (p=v->uv_addrs; p; p=p->pa_next) {
		    if ((src & p->pa_subnetmask) == p->pa_subnet &&
			((p->pa_subnetmask == 0xffffffff) ||
			 (src != p->pa_subnetbcast)))
			return(vifi);
		}
	    }
	}
    }
    return (NO_VIF);
}

static void
age_old_hosts()
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct listaddr *g;

    /*
     * Decrement the old-hosts-present timer for each
     * active group on each vif.
     */
    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++)
        for (g = v->uv_groups; g != NULL; g = g->al_next)
	    if (g->al_old)
		g->al_old--;
}


/*
 * Send group membership queries on each interface for which I am querier.
 * Note that technically, there should be a timer per interface, as the
 * dynamics of querier election can cause the "right" time to send a
 * query to be different on different interfaces.  However, this simple
 * implementation only ever sends queries sooner than the "right" time,
 * so can not cause loss of membership (but can send more packets than
 * necessary)
 */
void
query_groups()
{
    register vifi_t vifi;
    register struct uvif *v;

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {
	if (v->uv_flags & VIFF_QUERIER) {
	    send_query(v);
	}
    }
    age_old_hosts();
}

/*
 * Process an incoming host membership query.  Warn about
 * IGMP version mismatches, perform querier election, and
 * handle group-specific queries when we're not the querier.
 */
void
accept_membership_query(src, dst, group, tmo)
    u_int32 src, dst, group;
    int  tmo;
{
    register vifi_t vifi;
    register struct uvif *v;

    if ((vifi = find_vif(src, dst)) == NO_VIF ||
	(uvifs[vifi].uv_flags & VIFF_TUNNEL)) {
	log(LOG_INFO, 0,
	    "ignoring group membership query from non-adjacent host %s",
	    inet_fmt(src, s1));
	return;
    }

    v = &uvifs[vifi];

    if ((tmo == 0 && !(v->uv_flags & VIFF_IGMPV1)) ||
	(tmo != 0 &&  (v->uv_flags & VIFF_IGMPV1))) {
	int i;

	/*
	 * Exponentially back-off warning rate
	 */
	i = ++v->uv_igmpv1_warn;
	while (i && !(i & 1))
	    i >>= 1;
	if (i == 1)
	    log(LOG_WARNING, 0, "%s %s on vif %d, %s",
		tmo == 0 ? "Received IGMPv1 report from"
			 : "Received IGMPv2 report from",
		inet_fmt(src, s1),
		vifi,
		tmo == 0 ? "please configure vif for IGMPv1"
			 : "but I am configured for IGMPv1");
    }

    if (v->uv_querier == NULL || v->uv_querier->al_addr != src) {
	/*
	 * This might be:
	 * - A query from a new querier, with a lower source address
	 *   than the current querier (who might be me)
	 * - A query from a new router that just started up and doesn't
	 *   know who the querier is.
	 */
	if (ntohl(src) < (v->uv_querier ? ntohl(v->uv_querier->al_addr)
				   : ntohl(v->uv_lcl_addr))) {
	    IF_DEBUG(DEBUG_IGMP)
	    log(LOG_DEBUG, 0, "new querier %s (was %s) on vif %d",
		       inet_fmt(src, s1),
		       v->uv_querier ? inet_fmt(v->uv_querier->al_addr, s2) :
		       "me", vifi);
	    if (!v->uv_querier) {
		v->uv_querier = (struct listaddr *)
				       malloc(sizeof(struct listaddr));
		v->uv_flags &= ~VIFF_QUERIER;
	    }
	    time(&v->uv_querier->al_ctime);
	    v->uv_querier->al_addr = src;
	} else {
	    IF_DEBUG(DEBUG_IGMP)
	    log(LOG_DEBUG, 0, "ignoring query from %s; querier on vif %d is still %s",
		       inet_fmt(src, s1), vifi,
		       v->uv_querier ? inet_fmt(v->uv_querier->al_addr, s2) :
		       "me");

	    return;
	}
    }

    /*
     * Reset the timer since we've received a query.
     */
    if (v->uv_querier && src == v->uv_querier->al_addr)
        v->uv_querier->al_timer = 0;

    /*
     * If this is a Group-Specific query which we did not source,
     * we must set our membership timer to [Last Member Query Count] *
     * the [Max Response Time] in the packet.
     */
    if (!(v->uv_flags & (VIFF_IGMPV1|VIFF_QUERIER)) && group != 0 &&
					src != v->uv_lcl_addr) {
	register struct listaddr *g;

	IF_DEBUG(DEBUG_IGMP)
	log(LOG_DEBUG, 0,
	    "%s for %s from %s on vif %d, timer %d",
	    "Group-specific membership query",
	    inet_fmt(group, s2), inet_fmt(src, s1), vifi, tmo);
	
	for (g = v->uv_groups; g != NULL; g = g->al_next) {
	    if (group == g->al_addr && g->al_query == 0) {
		/* setup a timeout to remove the group membership */
		if (g->al_timerid)
		    g->al_timerid = DeleteTimer(g->al_timerid);
		g->al_timer = IGMP_LAST_MEMBER_QUERY_COUNT *
					   tmo / IGMP_TIMER_SCALE;
		/* use al_query to record our presence in last-member state */
		g->al_query = -1;
		g->al_timerid = SetTimer(vifi, g);
		IF_DEBUG(DEBUG_IGMP)
		log(LOG_DEBUG, 0,
		    "timer for grp %s on vif %d set to %d",
		    inet_fmt(group, s2), vifi, g->al_timer);
		break;
	    }
	}
    }
}

/*
 * Process an incoming group membership report.
 */
void
accept_group_report(src, dst, group, r_type)
    u_int32 src, dst, group;
    int  r_type;
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct listaddr *g;

    if ((vifi = find_vif(src, dst)) == NO_VIF ||
	(uvifs[vifi].uv_flags & VIFF_TUNNEL)) {
	log(LOG_INFO, 0,
	    "ignoring group membership report from non-adjacent host %s",
	    inet_fmt(src, s1));
	return;
    }

    v = &uvifs[vifi];

    /*
     * Look for the group in our group list; if found, reset its timer.
     */
    for (g = v->uv_groups; g != NULL; g = g->al_next) {
	if (group == g->al_addr) {
	    if (r_type == IGMP_V1_MEMBERSHIP_REPORT)
		g->al_old = OLD_AGE_THRESHOLD;

	    g->al_reporter = src;

	    /** delete old timers, set a timer for expiration **/
	    g->al_timer = IGMP_GROUP_MEMBERSHIP_INTERVAL;
	    if (g->al_query)
		g->al_query = DeleteTimer(g->al_query);
	    if (g->al_timerid)
		g->al_timerid = DeleteTimer(g->al_timerid);
	    g->al_timerid = SetTimer(vifi, g);	
	    break;
	}
    }

    /*
     * If not found, add it to the list and update kernel cache.
     */
    if (g == NULL) {
	g = (struct listaddr *)malloc(sizeof(struct listaddr));
	if (g == NULL)
	    log(LOG_ERR, 0, "ran out of memory");    /* fatal */

	g->al_addr   = group;
	if (r_type == IGMP_V1_MEMBERSHIP_REPORT)
	    g->al_old = OLD_AGE_THRESHOLD;
	else
	    g->al_old = 0;

	/** set a timer for expiration **/
        g->al_query	= 0;
	g->al_timer	= IGMP_GROUP_MEMBERSHIP_INTERVAL;
	g->al_reporter	= src;
	g->al_timerid	= SetTimer(vifi, g);
	g->al_next	= v->uv_groups;
	v->uv_groups	= g;
	time(&g->al_ctime);

	update_lclgrp(vifi, group);
    }

    /* 
     * Check if a graft is necessary for this group
     */
    chkgrp_graft(vifi, group);
}

/*
 * Process an incoming IGMPv2 Leave Group message.
 */
void
accept_leave_message(src, dst, group)
    u_int32 src, dst, group;
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct listaddr *g;

    if ((vifi = find_vif(src, dst)) == NO_VIF ||
	(uvifs[vifi].uv_flags & VIFF_TUNNEL)) {
	log(LOG_INFO, 0,
	    "ignoring group leave report from non-adjacent host %s",
	    inet_fmt(src, s1));
	return;
    }

    v = &uvifs[vifi];

    if (!(v->uv_flags & VIFF_QUERIER) || (v->uv_flags & VIFF_IGMPV1))
	return;

    /*
     * Look for the group in our group list in order to set up a short-timeout
     * query.
     */
    for (g = v->uv_groups; g != NULL; g = g->al_next) {
	if (group == g->al_addr) {
	    IF_DEBUG(DEBUG_IGMP)
	    log(LOG_DEBUG, 0,
		"[vif.c, _accept_leave_message] %d %d \n",
		g->al_old, g->al_query);

	    /* Ignore the leave message if there are old hosts present */
	    if (g->al_old)
		return;

	    /* still waiting for a reply to a query, ignore the leave */
	    if (g->al_query)
		return;

	    /** delete old timer set a timer for expiration **/
	    if (g->al_timerid)
		g->al_timerid = DeleteTimer(g->al_timerid);

#if IGMP_LAST_MEMBER_QUERY_COUNT != 2
This code needs to be updated to keep a counter of the number
of queries remaining.
#endif
	    /** send a group specific querry **/
	    g->al_timer = IGMP_LAST_MEMBER_QUERY_INTERVAL *
			(IGMP_LAST_MEMBER_QUERY_COUNT + 1);
	    send_igmp(v->uv_lcl_addr, g->al_addr,
		        IGMP_MEMBERSHIP_QUERY, 
		        IGMP_LAST_MEMBER_QUERY_INTERVAL * IGMP_TIMER_SCALE,
		        g->al_addr, 0);
	    g->al_query = SetQueryTimer(g, vifi,
			IGMP_LAST_MEMBER_QUERY_INTERVAL,
			IGMP_LAST_MEMBER_QUERY_INTERVAL * IGMP_TIMER_SCALE);
	    g->al_timerid = SetTimer(vifi, g);	
	    break;
	}
    }
}


/*
 * Send a periodic probe on all vifs.
 * Useful to determine one-way interfaces.
 * Detect neighbor loss faster.
 */
void
probe_for_neighbors()
{
    register vifi_t vifi;
    register struct uvif *v;

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {
	if (!(v->uv_flags & (VIFF_DOWN|VIFF_DISABLED))) {
	    send_probe_on_vif(v);
	}
    }
}


/*
 * Send a list of all of our neighbors to the requestor, `src'.
 */
void
accept_neighbor_request(src, dst)
    u_int32 src, dst;
{
    vifi_t vifi;
    struct uvif *v;
    u_char *p, *ncount;
    struct listaddr *la;
    int	datalen;
    u_int32 temp_addr, them = src;

#define PUT_ADDR(a)	temp_addr = ntohl(a); \
			*p++ = temp_addr >> 24; \
			*p++ = (temp_addr >> 16) & 0xFF; \
			*p++ = (temp_addr >> 8) & 0xFF; \
			*p++ = temp_addr & 0xFF;

    p = (u_char *) (send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
    datalen = 0;

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {
	if (v->uv_flags & VIFF_DISABLED)
	    continue;

	ncount = 0;

	for (la = v->uv_neighbors; la; la = la->al_next) {

	    /* Make sure that there's room for this neighbor... */
	    if (datalen + (ncount == 0 ? 4 + 3 + 4 : 4) > MAX_DVMRP_DATA_LEN) {
		send_igmp(INADDR_ANY, them, IGMP_DVMRP, DVMRP_NEIGHBORS,
			  htonl(MROUTED_LEVEL), datalen);
		p = (u_char *) (send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
		datalen = 0;
		ncount = 0;
	    }

	    /* Put out the header for this neighbor list... */
	    if (ncount == 0) {
		PUT_ADDR(v->uv_lcl_addr);
		*p++ = v->uv_metric;
		*p++ = v->uv_threshold;
		ncount = p;
		*p++ = 0;
		datalen += 4 + 3;
	    }

	    PUT_ADDR(la->al_addr);
	    datalen += 4;
	    (*ncount)++;
	}
    }

    if (datalen != 0)
	send_igmp(INADDR_ANY, them, IGMP_DVMRP, DVMRP_NEIGHBORS,
			htonl(MROUTED_LEVEL), datalen);
}

/*
 * Send a list of all of our neighbors to the requestor, `src'.
 */
void
accept_neighbor_request2(src, dst)
    u_int32 src, dst;
{
    vifi_t vifi;
    struct uvif *v;
    u_char *p, *ncount;
    struct listaddr *la;
    int	datalen;
    u_int32 them = src;

    p = (u_char *) (send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
    datalen = 0;

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {
	register u_short vflags = v->uv_flags;
	register u_char rflags = 0;
	if (vflags & VIFF_TUNNEL)
	    rflags |= DVMRP_NF_TUNNEL;
	if (vflags & VIFF_SRCRT)
	    rflags |= DVMRP_NF_SRCRT;
	if (vflags & VIFF_DOWN)
	    rflags |= DVMRP_NF_DOWN;
	if (vflags & VIFF_DISABLED)
	    rflags |= DVMRP_NF_DISABLED;
	if (vflags & VIFF_QUERIER)
	    rflags |= DVMRP_NF_QUERIER;
	if (vflags & VIFF_LEAF)
	    rflags |= DVMRP_NF_LEAF;
	ncount = 0;
	la = v->uv_neighbors;
	if (la == NULL) {
	    /*
	     * include down & disabled interfaces and interfaces on
	     * leaf nets.
	     */
	    if (rflags & DVMRP_NF_TUNNEL)
		rflags |= DVMRP_NF_DOWN;
	    if (datalen > MAX_DVMRP_DATA_LEN - 12) {
		send_igmp(INADDR_ANY, them, IGMP_DVMRP, DVMRP_NEIGHBORS2,
			  htonl(MROUTED_LEVEL), datalen);
		p = (u_char *) (send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
		datalen = 0;
	    }
	    *(u_int*)p = v->uv_lcl_addr;
	    p += 4;
	    *p++ = v->uv_metric;
	    *p++ = v->uv_threshold;
	    *p++ = rflags;
	    *p++ = 1;
	    *(u_int*)p =  v->uv_rmt_addr;
	    p += 4;
	    datalen += 12;
	} else {
	    for ( ; la; la = la->al_next) {
		/* Make sure that there's room for this neighbor... */
		if (datalen + (ncount == 0 ? 4+4+4 : 4) > MAX_DVMRP_DATA_LEN) {
		    send_igmp(INADDR_ANY, them, IGMP_DVMRP, DVMRP_NEIGHBORS2,
			      htonl(MROUTED_LEVEL), datalen);
		    p = (u_char *) (send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
		    datalen = 0;
		    ncount = 0;
		}
		/* Put out the header for this neighbor list... */
		if (ncount == 0) {
		    /* If it's a one-way tunnel, mark it down. */
		    if (rflags & DVMRP_NF_TUNNEL && la->al_flags & NBRF_ONEWAY)
			rflags |= DVMRP_NF_DOWN;
		    *(u_int*)p = v->uv_lcl_addr;
		    p += 4;
		    *p++ = v->uv_metric;
		    *p++ = v->uv_threshold;
		    *p++ = rflags;
		    ncount = p;
		    *p++ = 0;
		    datalen += 4 + 4;
		}
		/* Don't report one-way peering on phyint at all */
		if (!(rflags & DVMRP_NF_TUNNEL) && la->al_flags & NBRF_ONEWAY)
		    continue;
		*(u_int*)p = la->al_addr;
		p += 4;
		datalen += 4;
		(*ncount)++;
	    }
	    if (*ncount == 0) {
		*(u_int*)p = v->uv_rmt_addr;
		p += 4;
		datalen += 4;
		(*ncount)++;
	    }
	}
    }
    if (datalen != 0)
	send_igmp(INADDR_ANY, them, IGMP_DVMRP, DVMRP_NEIGHBORS2,
		htonl(MROUTED_LEVEL), datalen);
}

void
accept_info_request(src, dst, p, datalen)
    u_int32 src, dst;
    u_char *p;
    int datalen;
{
    u_char *q;
    int len;
    int outlen = 0;

    q = (u_char *) (send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);

    /* To be general, this must deal properly with breaking up over-sized
     * packets.  That implies passing a length to each function, and
     * allowing each function to request to be called again.  Right now,
     * we're only implementing the one thing we are positive will fit into
     * a single packet, so we wimp out.
     */
    while (datalen > 0) {
	len = 0;
	switch (*p) {
	    case DVMRP_INFO_VERSION:
		len = info_version(q, RECV_BUF_SIZE-(q-(u_char *)send_buf));
		break;

	    case DVMRP_INFO_NEIGHBORS:
	    default:
		log(LOG_INFO, 0, "ignoring unknown info type %d", *p);
		break;
	}
	*(q+1) = len++;
	outlen += len * 4;
	q += len * 4;
	len = (*(p+1) + 1) * 4;
	p += len;
	datalen -= len;
    }

    if (outlen != 0)
	send_igmp(INADDR_ANY, src, IGMP_DVMRP, DVMRP_INFO_REPLY,
			htonl(MROUTED_LEVEL), outlen);
}

/*
 * Information response -- return version string
 */
static int
info_version(p, plen)
    char *p;
    int plen;
{
    int len;
    extern char versionstring[];

    *p++ = DVMRP_INFO_VERSION;
    p++;	/* skip over length */
    *p++ = 0;	/* zero out */
    *p++ = 0;	/* reserved fields */
    strncpy(p, versionstring, plen - 4);
    p[plen-5] = '\0';

    len = strlen(versionstring);
    return ((len + 3) / 4);
}

/*
 * Process an incoming neighbor-list message.
 */
void
accept_neighbors(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    u_char *p;
    int datalen;
{
    log(LOG_INFO, 0, "ignoring spurious DVMRP neighbor list from %s to %s",
	inet_fmt(src, s1), inet_fmt(dst, s2));
}


/*
 * Process an incoming neighbor-list message.
 */
void
accept_neighbors2(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    u_char *p;
    int datalen;
{
    IF_DEBUG(DEBUG_PKT)
    log(LOG_DEBUG, 0, "ignoring spurious DVMRP neighbor list2 from %s to %s",
	inet_fmt(src, s1), inet_fmt(dst, s2));
}

/*
 * Process an incoming info reply message.
 */
void
accept_info_reply(src, dst, p, datalen)
    u_int32 src, dst;
    u_char *p;
    int datalen;
{
    IF_DEBUG(DEBUG_PKT)
    log(LOG_DEBUG, 0, "ignoring spurious DVMRP info reply from %s to %s",
	inet_fmt(src, s1), inet_fmt(dst, s2));
}


/*
 * Update the neighbor entry for neighbor 'addr' on vif 'vifi'.
 * 'msgtype' is the type of DVMRP message received from the neighbor.
 * Return the neighbor entry if 'addr' is a valid neighbor, FALSE otherwise.
 */
struct listaddr *
update_neighbor(vifi, addr, msgtype, p, datalen, level)
    vifi_t vifi;
    u_int32 addr;
    int msgtype;
    char *p;
    int datalen;
    u_int32 level;
{
    register struct uvif *v;
    register struct listaddr *n;
    int pv = level & 0xff;
    int mv = (level >> 8) & 0xff;
    int has_genid = 0;
    int in_router_list = 0;
    int dvmrpspec = 0;
    u_int32 genid;
    u_int32 send_tables = 0;
    int i;
    int do_reset = FALSE;

    v = &uvifs[vifi];

    /*
     * Confirm that 'addr' is a valid neighbor address on vif 'vifi'.
     * IT IS ASSUMED that this was preceded by a call to find_vif(), which
     * checks that 'addr' is either a valid remote tunnel endpoint or a
     * non-broadcast address belonging to a directly-connected subnet.
     * Therefore, here we check only that 'addr' is not our own address
     * (due to an impostor or erroneous loopback) or an address of the form
     * {subnet,0} ("the unknown host").  These checks are not performed in
     * find_vif() because those types of address are acceptable for some
     * types of IGMP message (such as group membership reports).
     */
    if (!(v->uv_flags & VIFF_TUNNEL) &&
	(addr == v->uv_lcl_addr ||
	 addr == v->uv_subnet )) {
	log(LOG_WARNING, 0,
	    "received DVMRP message from %s: %s",
	    (addr == v->uv_lcl_addr) ? "self (check device loopback)" :
				       "'the unknown host'",
	    inet_fmt(addr, s1));
	return NULL;
    }

    /*
     * Ignore all neighbors on vifs forced into leaf mode
     */
    if (v->uv_flags & VIFF_FORCE_LEAF) {
	return NULL;
    }

    /*
     * mrouted's version 3.3 and later include the generation ID
     * and the list of neighbors on the vif in their probe messages.
     */
    if (msgtype == DVMRP_PROBE && ((pv == 3 && mv > 2) ||
				   (pv > 3 && pv < 10))) {
	u_int32 router;

	IF_DEBUG(DEBUG_PEER)
	log(LOG_DEBUG, 0, "checking probe from %s (%d.%d) on vif %d",
	    inet_fmt(addr, s1), pv, mv, vifi);

	if (datalen < 4) {
	    log(LOG_WARNING, 0,
		"received truncated probe message from %s (len %d)",
		inet_fmt(addr, s1), datalen);
	    return NULL;
	}

	has_genid = 1;

	for (i = 0; i < 4; i++)
	  ((char *)&genid)[i] = *p++;
	datalen -= 4;

	while (datalen > 0) {
	    if (datalen < 4) {
		log(LOG_WARNING, 0,
		    "received truncated probe message from %s (len %d)",
		    inet_fmt(addr, s1), datalen);
		return NULL;
	    }
	    for (i = 0; i < 4; i++)
	      ((char *)&router)[i] = *p++;
	    datalen -= 4;

	    if (router == v->uv_lcl_addr) {
		in_router_list = 1;
		break;
	    }
	}
    }

    if ((pv == 3 && mv == 255) || (pv > 3 && pv < 10))
	dvmrpspec = 1;

    /*
     * Look for addr in list of neighbors.
     */
    for (n = v->uv_neighbors; n != NULL; n = n->al_next) {
	if (addr == n->al_addr) {
	    break;
	}
    }

    if (n == NULL) {
	/*
	 * New neighbor.
	 *
	 * If this neighbor follows the DVMRP spec, start the probe
	 * handshake.  If not, then it doesn't require the probe
	 * handshake, so establish the peering immediately.
	 */
	if (dvmrpspec && (msgtype != DVMRP_PROBE))
	    return NULL;

	for (i = 0; i < MAXNBRS; i++)
	    if (nbrs[i] == NULL)
		break;

	if (i == MAXNBRS) {
	    /* XXX This is a severe new restriction. */
	    /* XXX want extensible bitmaps! */
	    log(LOG_ERR, 0, "Can't handle %dth neighbor %s on vif %d!",
		MAXNBRS, inet_fmt(addr, s1), vifi);
	    /*NOTREACHED*/
	}

	/*
	 * Add it to our list of neighbors.
	 */
	IF_DEBUG(DEBUG_PEER)
	log(LOG_DEBUG, 0, "New neighbor %s on vif %d v%d.%d nf 0x%02x idx %d",
	    inet_fmt(addr, s1), vifi, level & 0xff, (level >> 8) & 0xff,
	    (level >> 16) & 0xff, i);

	n = (struct listaddr *)malloc(sizeof(struct listaddr));
	if (n == NULL)
	    log(LOG_ERR, 0, "ran out of memory");    /* fatal */

	n->al_addr      = addr;
	n->al_pv	= pv;
	n->al_mv	= mv;
	n->al_genid	= has_genid ? genid : 0;
	n->al_index	= i;
	nbrs[i] = n;

	time(&n->al_ctime);
	n->al_timer     = 0;
	n->al_flags	= has_genid ? NBRF_GENID : 0;
	n->al_next      = v->uv_neighbors;
	v->uv_neighbors = n;

	/*
	 * If we are not configured to peer with non-pruning routers,
	 * check the deprecated "I-know-how-to-prune" bit.  This bit
	 * was MBZ in early mrouted implementations (<3.5) and is required
	 * to be set by the DVMRPv3 specification.
	 */
	if (!(v->uv_flags & VIFF_ALLOW_NONPRUNERS) &&
	    !((level & 0x020000) || (pv == 3 && mv < 5))) {
	    n->al_flags |= NBRF_TOOOLD;
	}

	/*
	 * If this router implements the DVMRPv3 spec, then don't peer
	 * with him if we haven't yet established a bidirectional connection.
	 */
	if (dvmrpspec) {
	    if (!in_router_list) {
		IF_DEBUG(DEBUG_PEER)
		log(LOG_DEBUG, 0, "waiting for probe from %s with my addr",
		    inet_fmt(addr, s1));
		n->al_flags |= NBRF_WAITING;
		return NULL;
	    }
	}

	if (n->al_flags & NBRF_DONTPEER) {
	    IF_DEBUG(DEBUG_PEER)
	    log(LOG_DEBUG, 0, "not peering with %s on vif %d because %x",
		inet_fmt(addr, s1), vifi, n->al_flags & NBRF_DONTPEER);
	    return NULL;
	}

	/*
	 * If we thought that we had no neighbors on this vif, send a route
	 * report to the vif.  If this is just a new neighbor on the same
	 * vif, send the route report just to the new neighbor.
	 */
	if (NBRM_ISEMPTY(v->uv_nbrmap)) {
	    send_tables = v->uv_dst_addr;
	    vifs_with_neighbors++;
	} else {
	    send_tables = addr;
	}


	NBRM_SET(i, v->uv_nbrmap);
	add_neighbor_to_routes(vifi, i);
    } else {
	/*
	 * Found it.  Reset its timer.
	 */
	n->al_timer = 0;

	if (n->al_flags & NBRF_WAITING && msgtype == DVMRP_PROBE) {
	    n->al_flags &= ~NBRF_WAITING;
	    if (!in_router_list) {
		log(LOG_WARNING, 0, "possible one-way peering with %s on vif %d",
		    inet_fmt(addr, s1), vifi);
		n->al_flags |= NBRF_ONEWAY;
		return NULL;
	    } else {
		if (NBRM_ISEMPTY(v->uv_nbrmap)) {
		    send_tables = v->uv_dst_addr;
		    vifs_with_neighbors++;
		} else {
		    send_tables = addr;
		}
		NBRM_SET(n->al_index, v->uv_nbrmap);
		add_neighbor_to_routes(vifi, n->al_index);
		IF_DEBUG(DEBUG_PEER)
		log(LOG_DEBUG, 0, "%s on vif %d exits WAITING",
		    inet_fmt(addr, s1), vifi);
	    }
	}

	if (n->al_flags & NBRF_ONEWAY && msgtype == DVMRP_PROBE) {
	    if (in_router_list) {
		if (NBRM_ISEMPTY(v->uv_nbrmap))
		    vifs_with_neighbors++;
		NBRM_SET(n->al_index, v->uv_nbrmap);
		add_neighbor_to_routes(vifi, n->al_index);
		log(LOG_NOTICE, 0, "peering with %s on vif %d is no longer one-way",
			inet_fmt(addr, s1), vifi);
		n->al_flags &= ~NBRF_ONEWAY;
	    } else {
		/* XXX rate-limited warning message? */
		IF_DEBUG(DEBUG_PEER)
		log(LOG_DEBUG, 0, "%s on vif %d is still ONEWAY",
		    inet_fmt(addr, s1), vifi);
	    }
	}

	/*
	 * When peering with a genid-capable but pre-DVMRP spec peer,
	 * we might bring up the peering with a route report and not
	 * remember his genid.  Assume that he doesn't send a route
	 * report and then reboot before sending a probe.
	 */
	if (has_genid && !(n->al_flags & NBRF_GENID)) {
	    n->al_flags |= NBRF_GENID;
	    n->al_genid = genid;
	}

	/*
	 * update the neighbors version and protocol number and genid
	 * if changed => router went down and came up, 
	 * so take action immediately.
	 */
	if ((n->al_pv != pv) ||
	    (n->al_mv != mv) ||
	    (has_genid && n->al_genid != genid)) {

	    do_reset = TRUE;
	    IF_DEBUG(DEBUG_PEER)
	    log(LOG_DEBUG, 0,
		"version/genid change neighbor %s [old:%d.%d/%8x, new:%d.%d/%8x]",
		inet_fmt(addr, s1),
		n->al_pv, n->al_mv, n->al_genid, pv, mv, genid);
	    
	    n->al_pv = pv;
	    n->al_mv = mv;
	    n->al_genid = genid;
	    time(&n->al_ctime);
	}

	if ((pv == 3 && mv > 2) || (pv > 3 && pv < 10)) {
	    if (!(n->al_flags & VIFF_ONEWAY) && has_genid && !in_router_list &&
				(time(NULL) - n->al_ctime > 20)) {
		if (NBRM_ISSET(n->al_index, v->uv_nbrmap)) {
		    NBRM_CLR(n->al_index, v->uv_nbrmap);
		    if (NBRM_ISEMPTY(v->uv_nbrmap))
			vifs_with_neighbors--;
		}
		delete_neighbor_from_routes(addr, vifi, n->al_index);
		reset_neighbor_state(vifi, addr);
		log(LOG_WARNING, 0, "peering with %s on vif %d is one-way",
			inet_fmt(addr, s1), vifi);
		n->al_flags |= NBRF_ONEWAY;
	    }
	}

	if (n->al_flags & NBRF_DONTPEER) {
	    IF_DEBUG(DEBUG_PEER)
	    log(LOG_DEBUG, 0, "not peering with %s on vif %d because %x",
		inet_fmt(addr, s1), vifi, n->al_flags & NBRF_DONTPEER);
	    return NULL;
	}

	/* check "leaf" flag */
    }
    if (do_reset) {
	reset_neighbor_state(vifi, addr);
	if (!send_tables)
	    send_tables = addr;
    }
    if (send_tables) {
	send_probe_on_vif(v);
	report(ALL_ROUTES, vifi, send_tables);
    }
    v->uv_leaf_timer = 0;
    v->uv_flags &= ~VIFF_LEAF;

    return n;
}


/*
 * On every timer interrupt, advance the timer in each neighbor and
 * group entry on every vif.
 */
void
age_vifs()
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct listaddr *a, *prev_a;
    register u_int32 addr;

    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v ) {
	if (v->uv_leaf_timer && (v->uv_leaf_timer -= TIMER_INTERVAL == 0)) {
		v->uv_flags |= VIFF_LEAF;
	}

	for (prev_a = (struct listaddr *)&(v->uv_neighbors),
	     a = v->uv_neighbors;
	     a != NULL;
	     prev_a = a, a = a->al_next) {
	    int exp_time;
	    int idx;

	    if (((a->al_pv == 3) && (a->al_mv >= 3)) ||
		((a->al_pv > 3) && (a->al_pv < 10)))
		exp_time = NEIGHBOR_EXPIRE_TIME;
	    else
		exp_time = OLD_NEIGHBOR_EXPIRE_TIME;

	    if ((a->al_timer += TIMER_INTERVAL) < exp_time)
		continue;

	    IF_DEBUG(DEBUG_PEER)
	    log(LOG_DEBUG, 0, "Neighbor %s (%d.%d) expired after %d seconds",
		    inet_fmt(a->al_addr, s1), a->al_pv, a->al_mv, exp_time);

	    /*
	     * Neighbor has expired; delete it from the neighbor list,
	     * delete it from the 'dominants' and 'subordinates arrays of
	     * any route entries.
	     */
	    NBRM_CLR(a->al_index, v->uv_nbrmap);
	    nbrs[a->al_index] = NULL;	/* XXX is it a good idea to reuse indxs? */
	    idx = a->al_index;
	    addr = a->al_addr;
	    prev_a->al_next = a->al_next;
	    free((char *)a);
	    a = prev_a;/*XXX use ** */

	    delete_neighbor_from_routes(addr, vifi, idx);
	    reset_neighbor_state(vifi, addr);

	    if (NBRM_ISEMPTY(v->uv_nbrmap))
		vifs_with_neighbors--;

	    v->uv_leaf_timer = LEAF_CONFIRMATION_TIME;
	}

	if (v->uv_querier &&
	    (v->uv_querier->al_timer += TIMER_INTERVAL) >
		IGMP_OTHER_QUERIER_PRESENT_INTERVAL) {
	    /*
	     * The current querier has timed out.  We must become the
	     * querier.
	     */
	    IF_DEBUG(DEBUG_IGMP)
	    log(LOG_DEBUG, 0, "querier %s timed out",
		    inet_fmt(v->uv_querier->al_addr, s1));
	    free(v->uv_querier);
	    v->uv_querier = NULL;
	    v->uv_flags |= VIFF_QUERIER;
	    send_query(v);
	}
    }
}

/*
 * Returns the neighbor info struct for a given neighbor
 */
struct listaddr *
neighbor_info(vifi, addr)
    vifi_t vifi;
    u_int32 addr;
{
    struct listaddr *u;

    for (u = uvifs[vifi].uv_neighbors; u; u = u->al_next)
	if (u->al_addr == addr)
	    return u;

    return NULL;
}

static struct vnflags {
	int	vn_flag;
	char   *vn_name;
} vifflags[] = {
	{ VIFF_DOWN,		"down" },
	{ VIFF_DISABLED,	"disabled" },
	{ VIFF_QUERIER,		"querier" },
	{ VIFF_ONEWAY,		"one-way" },
	{ VIFF_LEAF,		"leaf" },
	{ VIFF_IGMPV1,		"IGMPv1" },
	{ VIFF_REXMIT_PRUNES,	"rexmit_prunes" },
	{ VIFF_PASSIVE,		"passive" },
	{ VIFF_ALLOW_NONPRUNERS,"allow_nonpruners" },
	{ VIFF_NOFLOOD,		"noflood" },
	{ VIFF_NOTRANSIT,	"notransit" },
	{ VIFF_BLASTER,		"blaster" },
	{ VIFF_FORCE_LEAF,	"force_leaf" },
	{ VIFF_OTUNNEL,		"old-tunnel" },
};

static struct vnflags nbrflags[] = {
	{ NBRF_LEAF,		"leaf" },
	{ NBRF_GENID,		"have-genid" },
	{ NBRF_WAITING,		"waiting" },
	{ NBRF_ONEWAY,		"one-way" },
	{ NBRF_TOOOLD,		"too old" },
	{ NBRF_TOOMANYROUTES,	"too many routes" },
	{ NBRF_NOTPRUNING,	"not pruning?" },
};

/*
 * Print the contents of the uvifs array on file 'fp'.
 */
void
dump_vifs(fp)
    FILE *fp;
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct listaddr *a;
    register struct phaddr *p;
    register struct vif_acl *acl;
    int i;
    struct sioc_vif_req v_req;
    time_t now;
    char *label;

    time(&now);
    fprintf(fp, "vifs_with_neighbors = %d\n", vifs_with_neighbors);

    if (vifs_with_neighbors == 1)
	fprintf(fp,"[This host is a leaf]\n\n");

    fprintf(fp,
    "\nVirtual Interface Table\n%s",
    "Vif  Name  Local-Address                               ");
    fprintf(fp,
    "M  Thr  Rate   Flags\n");

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {

	fprintf(fp, "%2u %6s  %-15s %6s: %-18s %2u %3u  %5u  ",
		vifi,
		v->uv_name,
		inet_fmt(v->uv_lcl_addr, s1),
		(v->uv_flags & VIFF_TUNNEL) ?
			"tunnel":
			"subnet",
		(v->uv_flags & VIFF_TUNNEL) ?
			inet_fmt(v->uv_rmt_addr, s2) :
			inet_fmts(v->uv_subnet, v->uv_subnetmask, s3),
		v->uv_metric,
		v->uv_threshold,
		v->uv_rate_limit);

	for (i = 0; i < sizeof(vifflags) / sizeof(struct vnflags); i++)
		if (v->uv_flags & vifflags[i].vn_flag)
			fprintf(fp, " %s", vifflags[i].vn_name);

	fprintf(fp, "\n");
	/*
	fprintf(fp, "                          #routes: %d\n", v->uv_nroutes);
	*/
	if (v->uv_admetric != 0)
	    fprintf(fp, "                                        advert-metric %2u\n",
		v->uv_admetric);

	label = "alternate subnets:";
	for (p = v->uv_addrs; p; p = p->pa_next) {
	    fprintf(fp, "                %18s %s\n", label,
			inet_fmts(p->pa_subnet, p->pa_subnetmask, s1));
	    label = "";
	}

	label = "peers:";
	for (a = v->uv_neighbors; a != NULL; a = a->al_next) {
	    fprintf(fp, "                            %6s %s (%d.%d) [%d]",
		    label, inet_fmt(a->al_addr, s1), a->al_pv, a->al_mv,
		    a->al_index);
	    for (i = 0; i < sizeof(nbrflags) / sizeof(struct vnflags); i++)
		    if (a->al_flags & nbrflags[i].vn_flag)
			    fprintf(fp, " %s", nbrflags[i].vn_name);
	    fprintf(fp, " up %s\n", scaletime(now - a->al_ctime));
	    /*fprintf(fp, " #routes %d\n", a->al_nroutes);*/
	    label = "";
	}

	label = "group host (time left):";
	for (a = v->uv_groups; a != NULL; a = a->al_next) {
	    fprintf(fp, "           %23s %-15s %-15s (%s)\n",
		    label,
		    inet_fmt(a->al_addr, s1),
		    inet_fmt(a->al_reporter, s2),
		    scaletime(timer_leftTimer(a->al_timerid)));
	    label = "";
	}
	label = "boundaries:";
	for (acl = v->uv_acl; acl != NULL; acl = acl->acl_next) {
	    fprintf(fp, "                       %11s %-18s\n", label,
			inet_fmts(acl->acl_addr, acl->acl_mask, s1));
	    label = "";
	}
	if (v->uv_filter) {
	    struct vf_element *vfe;
	    char lbuf[100];

	    sprintf(lbuf, "%5s %7s filter:",
			v->uv_filter->vf_flags & VFF_BIDIR ? "bidir"
							   : "     ",
			v->uv_filter->vf_type == VFT_ACCEPT ? "accept"
							    : "deny");
	    label = lbuf;
	    for (vfe = v->uv_filter->vf_filter;
					vfe != NULL; vfe = vfe->vfe_next) {
		fprintf(fp, "           %23s %-18s%s\n",
			label,
			inet_fmts(vfe->vfe_addr, vfe->vfe_mask, s1),
			vfe->vfe_flags & VFEF_EXACT ? " (exact)" : "");
		label = "";
	    }
	}
	if (!(v->uv_flags & (VIFF_TUNNEL|VIFF_DOWN|VIFF_DISABLED))) {
	    fprintf(fp, "                     IGMP querier: ");
	    if (v->uv_querier == NULL)
		if (v->uv_flags & VIFF_QUERIER)
		    fprintf(fp, "%-18s (this system)\n",
				    inet_fmt(v->uv_lcl_addr, s1));
		else
		    fprintf(fp, "NONE - querier election failure?\n");
	    else
	    	fprintf(fp, "%-18s up %s last heard %s ago\n",
			inet_fmt(v->uv_querier->al_addr, s1),
			scaletime(now - v->uv_querier->al_ctime),
			scaletime(v->uv_querier->al_timer));
	}
	if (v->uv_flags & VIFF_BLASTER)
	    fprintf(fp, "                  blasterbuf size: %dk\n",
			v->uv_blasterlen / 1024);
	fprintf(fp, "                      Nbr bitmaps: 0x%08lx%08lx\n",/*XXX*/
			v->uv_nbrmap.hi, v->uv_nbrmap.lo);
	if (v->uv_prune_lifetime != 0)
	    fprintf(fp, "                   Prune Lifetime: %d seconds\n",
					    v->uv_prune_lifetime);

	v_req.vifi = vifi;
	if (did_final_init)
	    if (ioctl(udp_socket, SIOCGETVIFCNT, (char *)&v_req) < 0) {
		log(LOG_WARNING, errno,
		    "SIOCGETVIFCNT fails on vif %d", vifi);
	    } else {
		fprintf(fp, "                   pkts/bytes in : %lu/%lu\n",
			v_req.icount, v_req.ibytes);
		fprintf(fp, "                   pkts/bytes out: %lu/%lu\n",
			v_req.ocount, v_req.obytes);
	    }
	fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}

/*
 * Time out record of a group membership on a vif
 */
static void
DelVif(arg)
    void *arg;
{
    cbk_t *cbk = (cbk_t *)arg;
    vifi_t vifi = cbk->vifi;
    struct uvif *v = &uvifs[vifi];
    struct listaddr *a, **anp, *g = cbk->g;

    /*
     * Group has expired
     * delete all kernel cache entries with this group
     */
    if (g->al_query)
	DeleteTimer(g->al_query);

    delete_lclgrp(vifi, g->al_addr);

    anp = &(v->uv_groups);
    while ((a = *anp) != NULL) {
    	if (a == g) {
	    *anp = a->al_next;
	    free((char *)a);
	} else {
	    anp = &a->al_next;
	}
    }

    free(cbk);
}

/*
 * Set a timer to delete the record of a group membership on a vif.
 */
static int
SetTimer(vifi, g)
    vifi_t vifi;
    struct listaddr *g;
{
    cbk_t *cbk;

    cbk = (cbk_t *) malloc(sizeof(cbk_t));
    cbk->g = g;
    cbk->vifi = vifi;
    return timer_setTimer(g->al_timer, DelVif, cbk);
}

/*
 * Delete a timer that was set above.
 */
static int
DeleteTimer(id)
    int id;
{
    timer_clearTimer(id);
    return 0;
}

/*
 * Send a group-specific query.
 */
static void
SendQuery(arg)
    void *arg;
{
    cbk_t *cbk = (cbk_t *)arg;
    register struct uvif *v = &uvifs[cbk->vifi];

    send_igmp(v->uv_lcl_addr, cbk->g->al_addr,
	      IGMP_MEMBERSHIP_QUERY,
	      cbk->q_time, cbk->g->al_addr, 0);
    cbk->g->al_query = 0;
    free(cbk);
}

/*
 * Set a timer to send a group-specific query.
 */
static int
SetQueryTimer(g, vifi, to_expire, q_time)
    struct listaddr *g;
    vifi_t vifi;
    int to_expire, q_time;
{
    cbk_t *cbk;

    cbk = (cbk_t *) malloc(sizeof(cbk_t));
    cbk->g = g;
    cbk->q_time = q_time;
    cbk->vifi = vifi;
    return timer_setTimer(to_expire, SendQuery, cbk);
}
