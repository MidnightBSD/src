/*-
 * Copyright (c) 2004-2005 Robert N. M. Watson
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 * $FreeBSD: src/sys/netatalk/ddp_pcb.c,v 1.47 2005/01/07 02:35:34 imp Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp_var.h>
#include <netatalk/ddp_pcb.h>
#include <netatalk/at_extern.h>

struct mtx		 ddp_list_mtx;
static struct ddpcb	*ddp_ports[ ATPORT_LAST ];
struct ddpcb		*ddpcb_list = NULL;

void
at_sockaddr(struct ddpcb *ddp, struct sockaddr **addr)
{

    /*
     * Prevent modification of ddp during copy of addr.
     */
    DDP_LOCK_ASSERT(ddp);
    *addr = sodupsockaddr((struct sockaddr *)&ddp->ddp_lsat, M_NOWAIT);
}

int 
at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr *addr, struct thread *td)
{
    struct sockaddr_at	lsat, *sat;
    struct at_ifaddr	*aa;
    struct ddpcb	*ddpp;

    /*
     * We read and write both the ddp passed in, and also ddp_ports.
     */
    DDP_LIST_XLOCK_ASSERT();
    DDP_LOCK_ASSERT(ddp);

    if (ddp->ddp_lsat.sat_port != ATADDR_ANYPORT) { /* shouldn't be bound */
	return (EINVAL);
    }

    if (addr != NULL) {			/* validate passed address */
	sat = (struct sockaddr_at *)addr;
	if (sat->sat_family != AF_APPLETALK) {
	    return (EAFNOSUPPORT);
	}

	if (sat->sat_addr.s_node != ATADDR_ANYNODE ||
		sat->sat_addr.s_net != ATADDR_ANYNET) {
	    for (aa = at_ifaddr_list; aa != NULL; aa = aa->aa_next) {
		if ((sat->sat_addr.s_net == AA_SAT(aa)->sat_addr.s_net) &&
		 (sat->sat_addr.s_node == AA_SAT(aa)->sat_addr.s_node)) {
		    break;
		}
	    }
	    if (!aa) {
		return (EADDRNOTAVAIL);
	    }
	}

	if (sat->sat_port != ATADDR_ANYPORT) {
	    if (sat->sat_port < ATPORT_FIRST ||
		    sat->sat_port >= ATPORT_LAST) {
		return (EINVAL);
	    }
	    if (sat->sat_port < ATPORT_RESERVED &&
		 suser(td)) {
		return (EACCES);
	    }
	}
    } else {
	bzero((caddr_t)&lsat, sizeof(struct sockaddr_at));
	lsat.sat_len = sizeof(struct sockaddr_at);
	lsat.sat_addr.s_node = ATADDR_ANYNODE;
	lsat.sat_addr.s_net = ATADDR_ANYNET;
	lsat.sat_family = AF_APPLETALK;
	sat = &lsat;
    }

    if (sat->sat_addr.s_node == ATADDR_ANYNODE &&
	    sat->sat_addr.s_net == ATADDR_ANYNET) {
	if (at_ifaddr_list == NULL) {
	    return (EADDRNOTAVAIL);
	}
	sat->sat_addr = AA_SAT(at_ifaddr_list)->sat_addr;
    }
    ddp->ddp_lsat = *sat;

    /*
     * Choose port.
     */
    if (sat->sat_port == ATADDR_ANYPORT) {
	for (sat->sat_port = ATPORT_RESERVED;
		sat->sat_port < ATPORT_LAST; sat->sat_port++) {
	    if (ddp_ports[ sat->sat_port - 1 ] == NULL) {
		break;
	    }
	}
	if (sat->sat_port == ATPORT_LAST) {
	    return (EADDRNOTAVAIL);
	}
	ddp->ddp_lsat.sat_port = sat->sat_port;
	ddp_ports[ sat->sat_port - 1 ] = ddp;
    } else {
	for (ddpp = ddp_ports[ sat->sat_port - 1 ]; ddpp;
		ddpp = ddpp->ddp_pnext) {
	    if (ddpp->ddp_lsat.sat_addr.s_net == sat->sat_addr.s_net &&
		    ddpp->ddp_lsat.sat_addr.s_node == sat->sat_addr.s_node) {
		break;
	    }
	}
	if (ddpp != NULL) {
	    return (EADDRINUSE);
	}
	ddp->ddp_pnext = ddp_ports[ sat->sat_port - 1 ];
	ddp_ports[ sat->sat_port - 1 ] = ddp;
	if (ddp->ddp_pnext) {
	    ddp->ddp_pnext->ddp_pprev = ddp;
	}
    }

    return (0);
}

int
at_pcbconnect(struct ddpcb *ddp, struct sockaddr *addr, struct thread *td)
{
    struct sockaddr_at	*sat = (struct sockaddr_at *)addr;
    struct route	*ro;
    struct at_ifaddr	*aa = NULL;
    struct ifnet	*ifp;
    u_short		hintnet = 0, net;

    DDP_LIST_XLOCK_ASSERT();
    DDP_LOCK_ASSERT(ddp);

    if (sat->sat_family != AF_APPLETALK) {
	return (EAFNOSUPPORT);
    }

    /*
     * Under phase 2, network 0 means "the network".  We take "the
     * network" to mean the network the control block is bound to.
     * If the control block is not bound, there is an error.
     */
    if (sat->sat_addr.s_net == ATADDR_ANYNET
		&& sat->sat_addr.s_node != ATADDR_ANYNODE) {
	if (ddp->ddp_lsat.sat_port == ATADDR_ANYPORT) {
	    return (EADDRNOTAVAIL);
	}
	hintnet = ddp->ddp_lsat.sat_addr.s_net;
    }

    ro = &ddp->ddp_route;
    /*
     * If we've got an old route for this pcb, check that it is valid.
     * If we've changed our address, we may have an old "good looking"
     * route here.  Attempt to detect it.
     */
    if (ro->ro_rt) {
	if (hintnet) {
	    net = hintnet;
	} else {
	    net = sat->sat_addr.s_net;
	}
	aa = NULL;
	if ((ifp = ro->ro_rt->rt_ifp) != NULL) {
	    for (aa = at_ifaddr_list; aa != NULL; aa = aa->aa_next) {
		if (aa->aa_ifp == ifp &&
			ntohs(net) >= ntohs(aa->aa_firstnet) &&
			ntohs(net) <= ntohs(aa->aa_lastnet)) {
		    break;
		}
	    }
	}
	if (aa == NULL || (satosat(&ro->ro_dst)->sat_addr.s_net !=
		(hintnet ? hintnet : sat->sat_addr.s_net) ||
		satosat(&ro->ro_dst)->sat_addr.s_node !=
		sat->sat_addr.s_node)) {
	    RTFREE(ro->ro_rt);
	    ro->ro_rt = NULL;
	}
    }

    /*
     * If we've got no route for this interface, try to find one.
     */
    if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL) {
	ro->ro_dst.sa_len = sizeof(struct sockaddr_at);
	ro->ro_dst.sa_family = AF_APPLETALK;
	if (hintnet) {
	    satosat(&ro->ro_dst)->sat_addr.s_net = hintnet;
	} else {
	    satosat(&ro->ro_dst)->sat_addr.s_net = sat->sat_addr.s_net;
	}
	satosat(&ro->ro_dst)->sat_addr.s_node = sat->sat_addr.s_node;
	rtalloc(ro);
    }

    /*
     * Make sure any route that we have has a valid interface.
     */
    aa = NULL;
    if (ro->ro_rt && (ifp = ro->ro_rt->rt_ifp)) {
	for (aa = at_ifaddr_list; aa != NULL; aa = aa->aa_next) {
	    if (aa->aa_ifp == ifp) {
		break;
	    }
	}
    }
    if (aa == NULL) {
	return (ENETUNREACH);
    }

    ddp->ddp_fsat = *sat;
    if (ddp->ddp_lsat.sat_port == ATADDR_ANYPORT) {
	return (at_pcbsetaddr(ddp, NULL, td));
    }
    return (0);
}

void 
at_pcbdisconnect(struct ddpcb	*ddp)
{

    DDP_LOCK_ASSERT(ddp);

    ddp->ddp_fsat.sat_addr.s_net = ATADDR_ANYNET;
    ddp->ddp_fsat.sat_addr.s_node = ATADDR_ANYNODE;
    ddp->ddp_fsat.sat_port = ATADDR_ANYPORT;
}

int
at_pcballoc(struct socket *so)
{
	struct ddpcb	*ddp;

	DDP_LIST_XLOCK_ASSERT();

	MALLOC(ddp, struct ddpcb *, sizeof *ddp, M_PCB, M_NOWAIT | M_ZERO);
	if (ddp == NULL)
		return (ENOBUFS);
	DDP_LOCK_INIT(ddp);
	ddp->ddp_lsat.sat_port = ATADDR_ANYPORT;

	ddp->ddp_socket = so;
	so->so_pcb = (caddr_t)ddp;

	ddp->ddp_next = ddpcb_list;
	ddp->ddp_prev = NULL;
	ddp->ddp_pprev = NULL;
	ddp->ddp_pnext = NULL;
	if (ddpcb_list != NULL) {
		ddpcb_list->ddp_prev = ddp;
	}
	ddpcb_list = ddp;
	return(0);
}

void
at_pcbdetach(struct socket *so, struct ddpcb *ddp)
{

    /*
     * We modify ddp, ddp_ports, and the global list.
     */
    DDP_LIST_XLOCK_ASSERT();
    DDP_LOCK_ASSERT(ddp);

    soisdisconnected(so);
    ACCEPT_LOCK();
    SOCK_LOCK(so);
    so->so_pcb = NULL;
    sotryfree(so);

    /* remove ddp from ddp_ports list */
    if (ddp->ddp_lsat.sat_port != ATADDR_ANYPORT &&
	    ddp_ports[ ddp->ddp_lsat.sat_port - 1 ] != NULL) {
	if (ddp->ddp_pprev != NULL) {
	    ddp->ddp_pprev->ddp_pnext = ddp->ddp_pnext;
	} else {
	    ddp_ports[ ddp->ddp_lsat.sat_port - 1 ] = ddp->ddp_pnext;
	}
	if (ddp->ddp_pnext != NULL) {
	    ddp->ddp_pnext->ddp_pprev = ddp->ddp_pprev;
	}
    }

    if (ddp->ddp_route.ro_rt) {
	RTFREE(ddp->ddp_route.ro_rt);
    }

    if (ddp->ddp_prev) {
	ddp->ddp_prev->ddp_next = ddp->ddp_next;
    } else {
	ddpcb_list = ddp->ddp_next;
    }
    if (ddp->ddp_next) {
	ddp->ddp_next->ddp_prev = ddp->ddp_prev;
    }
    DDP_UNLOCK(ddp);
    DDP_LOCK_DESTROY(ddp);
    FREE(ddp, M_PCB);
}

/*
 * For the moment, this just find the pcb with the correct local address.
 * In the future, this will actually do some real searching, so we can use
 * the sender's address to do de-multiplexing on a single port to many
 * sockets (pcbs).
 */
struct ddpcb *
ddp_search(struct sockaddr_at *from, struct sockaddr_at *to,
			struct at_ifaddr *aa)
{
    struct ddpcb	*ddp;

    DDP_LIST_SLOCK_ASSERT();

    /*
     * Check for bad ports.
     */
    if (to->sat_port < ATPORT_FIRST || to->sat_port >= ATPORT_LAST) {
	return (NULL);
    }

    /*
     * Make sure the local address matches the sent address.  What about
     * the interface?
     */
    for (ddp = ddp_ports[ to->sat_port - 1 ]; ddp; ddp = ddp->ddp_pnext) {
	DDP_LOCK(ddp);
	/* XXX should we handle 0.YY? */

	/* XXXX.YY to socket on destination interface */
	if (to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net &&
		to->sat_addr.s_node == ddp->ddp_lsat.sat_addr.s_node) {
	    DDP_UNLOCK(ddp);
	    break;
	}

	/* 0.255 to socket on receiving interface */
	if (to->sat_addr.s_node == ATADDR_BCAST && (to->sat_addr.s_net == 0 ||
		to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net) &&
		ddp->ddp_lsat.sat_addr.s_net == AA_SAT(aa)->sat_addr.s_net) {
	    DDP_UNLOCK(ddp);
	    break;
	}

	/* XXXX.0 to socket on destination interface */
	if (to->sat_addr.s_net == aa->aa_firstnet &&
		to->sat_addr.s_node == 0 &&
		ntohs(ddp->ddp_lsat.sat_addr.s_net) >=
		ntohs(aa->aa_firstnet) &&
		ntohs(ddp->ddp_lsat.sat_addr.s_net) <=
		ntohs(aa->aa_lastnet)) {
	    DDP_UNLOCK(ddp);
	    break;
	}
	DDP_UNLOCK(ddp);
    }
    return (ddp);
}
