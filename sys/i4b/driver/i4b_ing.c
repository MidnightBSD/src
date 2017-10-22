/*-
 * Copyright (c) 1999, 2002 Hellmuth Michaelis. All rights reserved.
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

/*---------------------------------------------------------------------------
 *
 *	i4b_ing.c - isdn4bsd B-channel to netgraph driver
 *	-------------------------------------------------
 *	last edit-date: [Sat Mar  9 14:09:53 2002]
 *
 *---------------------------------------------------------------------------*/ 

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/i4b/driver/i4b_ing.c 171270 2007-07-06 07:17:22Z bz $");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <net/if.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/netgraph.h>

#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_debug.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer4/i4b_l4.h>

#error "Cannot be used until I4B is locked."

#define I4BINGACCT	1		/* enable accounting messages */
#define	I4BINGACCTINTVL	2		/* accounting msg interval in secs */

#define I4BINGMAXQLEN	50		/* max queue length */

/* initialized by L4 */

static drvr_link_t ing_drvr_linktab[NI4BING];
static isdn_link_t *isdn_linktab[NI4BING];

struct ing_softc {
	int		sc_unit;	/* unit number			*/
	int		sc_state;	/* state of the interface	*/
	call_desc_t	*sc_cdp;	/* ptr to call descriptor	*/
	int		sc_updown;	/* soft state of interface	*/
	struct ifqueue  sc_fastq;	/* interactive traffic		*/
	int		sc_dialresp;	/* dialresponse			*/
	int		sc_lastdialresp;/* last dialresponse		*/
	
#if I4BINGACCT
	struct callout_handle sc_callout;
	int		sc_iinb;	/* isdn driver # of inbytes	*/
	int		sc_ioutb;	/* isdn driver # of outbytes	*/
	int		sc_inb;		/* # of bytes rx'd		*/
	int		sc_outb;	/* # of bytes tx'd	 	*/
	int		sc_linb;	/* last # of bytes rx'd		*/
	int		sc_loutb;	/* last # of bytes tx'd 	*/
	int		sc_fn;		/* flag, first null acct	*/
#endif	

	int		sc_inpkt;	/* incoming packets		*/
	int		sc_outpkt;	/* outgoing packets		*/	

	struct ifqueue  xmitq_hipri;    /* hi-priority transmit queue */
	struct ifqueue  xmitq;	  /* transmit queue */
		
	node_p		node;		/* back pointer to node */
	char		nodename[NG_NODESIZ]; /* store our node name */
	hook_p  	debughook;
	hook_p  	hook;	

	u_int   	packets_in;	/* packets in from downstream */
	u_int   	packets_out;	/* packets out towards downstream */
	u_int32_t	flags;

} ing_softc[NI4BING];

enum ing_states {
	ST_IDLE,			/* initialized, ready, idle	*/
	ST_DIALING,			/* dialling out to remote	*/
	ST_CONNECTED			/* connected to remote		*/
};

static void i4bingattach(void *);

PSEUDO_SET(i4bingattach, i4b_ing);

static void ing_init_linktab(int unit);
static void ing_tx_queue_empty(int unit);

/* ========= NETGRAPH ============= */

#define NG_ING_NODE_TYPE	"i4bing"	/* node type name */
#define NGM_ING_COOKIE		947513046	/* node type cookie */

/* Hook names */
#define NG_ING_HOOK_DEBUG	"debug"
#define NG_ING_HOOK_RAW		"rawdata"

/* Netgraph commands understood by this node type */
enum {
	NGM_ING_SET_FLAG = 1,
	NGM_ING_GET_STATUS,
};

/* This structure is returned by the NGM_ING_GET_STATUS command */
struct ngingstat {
	u_int   packets_in;	/* packets in from downstream */
	u_int   packets_out;	/* packets out towards downstream */
};

/*
 * This is used to define the 'parse type' for a struct ngingstat, which
 * is bascially a description of how to convert a binary struct ngingstat
 * to an ASCII string and back.  See ng_parse.h for more info.
 *
 * This needs to be kept in sync with the above structure definition
 */
#define NG_ING_STATS_TYPE_INFO	{				\
	  { "packets_in",	&ng_parse_int32_type	},	\
	  { "packets_out",	&ng_parse_int32_type	},	\
	  { NULL },						\
}

/*
 * This section contains the netgraph method declarations for the
 * sample node. These methods define the netgraph 'type'.
 */

static ng_constructor_t	ng_ing_constructor;
static ng_rcvmsg_t	ng_ing_rcvmsg;
static ng_shutdown_t	ng_ing_shutdown;
static ng_newhook_t	ng_ing_newhook;
static ng_connect_t	ng_ing_connect;
static ng_rcvdata_t	ng_ing_rcvdata;
static ng_disconnect_t	ng_ing_disconnect;

/* Parse type for struct ngingstat */
static const struct
	ng_parse_struct_field ng_ing_stat_type_fields[] =
	NG_ING_STATS_TYPE_INFO;

static const struct ng_parse_type ng_ing_stat_type = {
	&ng_parse_struct_type,
	&ng_ing_stat_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */

static const struct ng_cmdlist ng_ing_cmdlist[] = {
	{
		NGM_ING_COOKIE,
		NGM_ING_GET_STATUS,
		"getstatus",
		NULL,
		&ng_ing_stat_type,
	},
	{
		NGM_ING_COOKIE,
		NGM_ING_SET_FLAG,
		"setflag",
		&ng_parse_int32_type,
		NULL
	},
	{ 0 }
};

/* Netgraph node type descriptor */
static struct ng_type typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_ING_NODE_TYPE,
	.constructor =	ng_ing_constructor,
	.rcvmsg =	ng_ing_rcvmsg,
	.shutdown =	ng_ing_shutdown,
	.newhook =	ng_ing_newhook,
	.connect =	ng_ing_connect,
	.rcvdata =	ng_ing_rcvdata,
	.disconnect =	ng_ing_disconnect,
	.cmdlist =	ng_ing_cmdlist,
};

NETGRAPH_INIT_ORDERED(ing, &typestruct, SI_SUB_DRIVERS, SI_ORDER_ANY);

/*===========================================================================*
 *			DEVICE DRIVER ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	interface attach routine at kernel boot time
 *---------------------------------------------------------------------------*/
static void
i4bingattach(void *dummy)
{
	struct ing_softc *sc = ing_softc;
	int i;
	int ret;

	printf("i4bing: %d i4b NetGraph ISDN B-channel device(s) attached\n", NI4BING);
	
	for(i=0; i < NI4BING; sc++, i++)
	{
		sc->sc_unit = i;
		
		ing_init_linktab(i);

		NDBGL4(L4_DIALST, "setting dial state to ST_IDLE");

		sc->sc_state = ST_IDLE;
		
		sc->sc_fastq.ifq_maxlen = I4BINGMAXQLEN;
		if(!mtx_initialized(&sc->sc_fastq.ifq_mtx))
			mtx_init(&sc->sc_fastq.ifq_mtx, "i4b_ing_fastq", NULL, MTX_DEF);
		
#if I4BINGACCT
		callout_handle_init(&sc->sc_callout);
		sc->sc_iinb = 0;
		sc->sc_ioutb = 0;
		sc->sc_inb = 0;
		sc->sc_outb = 0;
		sc->sc_linb = 0;
		sc->sc_loutb = 0;
		sc->sc_fn = 1;
#endif

		sc->sc_inpkt = 0;
		sc->sc_outpkt = 0;		

		sc->sc_updown = SOFT_ENA;	/* soft enabled */

		sc->sc_dialresp = DSTAT_NONE;	/* no response */
		sc->sc_lastdialresp = DSTAT_NONE;
		
		/* setup a netgraph node */

		if ((ret = ng_make_node_common(&typestruct, &sc->node)))
		{
			printf("ing: ng_make_node_common, ret = %d\n!", ret);
		}

		/* name the netgraph node */

		sprintf(sc->nodename, "%s%d", NG_ING_NODE_TYPE, sc->sc_unit);
		if((ret = ng_name_node(sc->node, sc->nodename)))
		{
			printf("ing: ng_name node, ret = %d\n!", ret);
			NG_NODE_UNREF(sc->node);
			break;
		}

		NG_NODE_SET_PRIVATE(sc->node, sc);

		sc->xmitq.ifq_maxlen = IFQ_MAXLEN;
		sc->xmitq_hipri.ifq_maxlen = IFQ_MAXLEN;
		if(!mtx_initialized(&sc->xmitq.ifq_mtx))
			mtx_init(&sc->xmitq.ifq_mtx, "i4b_ing_xmitq", NULL, MTX_DEF);
		if(!mtx_initialized(&sc->xmitq_hipri.ifq_mtx))
			mtx_init(&sc->xmitq_hipri.ifq_mtx, "i4b_ing_hipri", NULL, MTX_DEF);
	}
}

#ifdef I4BINGACCT
/*---------------------------------------------------------------------------*
 *	accounting timeout routine
 *---------------------------------------------------------------------------*/
static void
ing_timeout(struct ing_softc *sc)
{
	bchan_statistics_t bs;
	int unit = sc->sc_unit;

	/* get # of bytes in and out from the HSCX driver */ 
	
	(*isdn_linktab[unit]->bch_stat)
		(isdn_linktab[unit]->unit, isdn_linktab[unit]->channel, &bs);

	sc->sc_ioutb += bs.outbytes;
	sc->sc_iinb += bs.inbytes;
	
	if((sc->sc_iinb != sc->sc_linb) || (sc->sc_ioutb != sc->sc_loutb) || sc->sc_fn) 
	{
		int ri = (sc->sc_iinb - sc->sc_linb)/I4BINGACCTINTVL;
		int ro = (sc->sc_ioutb - sc->sc_loutb)/I4BINGACCTINTVL;

		if((sc->sc_iinb == sc->sc_linb) && (sc->sc_ioutb == sc->sc_loutb))
			sc->sc_fn = 0;
		else
			sc->sc_fn = 1;
			
		sc->sc_linb = sc->sc_iinb;
		sc->sc_loutb = sc->sc_ioutb;

		i4b_l4_accounting(BDRV_ING, unit, ACCT_DURING,
			 sc->sc_ioutb, sc->sc_iinb, ro, ri, sc->sc_ioutb, sc->sc_iinb);
 	}

	sc->sc_callout = timeout((TIMEOUT_FUNC_T)ing_timeout,
					(void *)sc, I4BINGACCTINTVL*hz);
}
#endif /* I4BINGACCT */

#if 0
/*---------------------------------------------------------------------------*
 *	clear the interface's send queues
 *---------------------------------------------------------------------------*/
static void
ingclearqueue(struct ifqueue *iq)
{
	int x;
	
	x = splimp();
	IF_DRAIN(iq);
	splx(x);
}
#endif

/*===========================================================================*
 *			ISDN INTERFACE ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at connect time
 *---------------------------------------------------------------------------*/
static void
ing_connect(int unit, void *cdp)
{
	struct ing_softc *sc = &ing_softc[unit];
	int s;

	sc->sc_cdp = (call_desc_t *)cdp;

	s = SPLI4B();

	NDBGL4(L4_DIALST, "ing%d: setting dial state to ST_CONNECTED", unit);

	sc->sc_dialresp = DSTAT_NONE;
	sc->sc_lastdialresp = DSTAT_NONE;	
	
#if I4BINGACCT
	sc->sc_iinb = 0;
	sc->sc_ioutb = 0;
	sc->sc_inb = 0;
	sc->sc_outb = 0;
	sc->sc_linb = 0;
	sc->sc_loutb = 0;
	sc->sc_callout = timeout((TIMEOUT_FUNC_T)ing_timeout,
				(void *)sc, I4BINGACCTINTVL*hz);
#endif

	sc->sc_state = ST_CONNECTED;
	
	splx(s);
}
	
/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at disconnect time
 *---------------------------------------------------------------------------*/
static void
ing_disconnect(int unit, void *cdp)
{
	call_desc_t *cd = (call_desc_t *)cdp;
	struct ing_softc *sc = &ing_softc[unit];

	/* new stuff to check that the active channel is being closed */

	if (cd != sc->sc_cdp)
	{
		NDBGL4(L4_INGDBG, "ing%d: channel %d not active",
				cd->driver_unit, cd->channelid);
		return;
	}

#if I4BINGACCT
	untimeout((TIMEOUT_FUNC_T)ing_timeout,
		(void *)sc, sc->sc_callout);
#endif

	i4b_l4_accounting(BDRV_ING, cd->driver_unit, ACCT_FINAL,
		 sc->sc_ioutb, sc->sc_iinb, 0, 0, sc->sc_outb, sc->sc_inb);
	
	sc->sc_cdp = (call_desc_t *)0;	

	NDBGL4(L4_DIALST, "setting dial state to ST_IDLE");

	sc->sc_dialresp = DSTAT_NONE;
	sc->sc_lastdialresp = DSTAT_NONE;	

	sc->sc_state = ST_IDLE;
}

/*---------------------------------------------------------------------------*
 *	this routine is used to give a feedback from userland daemon
 *	in case of dial problems
 *---------------------------------------------------------------------------*/
static void
ing_dialresponse(int unit, int status, cause_t cause)
{
	struct ing_softc *sc = &ing_softc[unit];
	sc->sc_dialresp = status;

	NDBGL4(L4_INGDBG, "ing%d: last=%d, this=%d",
		unit, sc->sc_lastdialresp, sc->sc_dialresp);

	if(status != DSTAT_NONE)
	{
		NDBGL4(L4_INGDBG, "ing%d: clearing queues", unit);
/*		ingclearqueues(sc); */
	}
}
	
/*---------------------------------------------------------------------------*
 *	interface soft up/down
 *---------------------------------------------------------------------------*/
static void
ing_updown(int unit, int updown)
{
	struct ing_softc *sc = &ing_softc[unit];
	sc->sc_updown = updown;
}
	
/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when a new frame (mbuf) has been received and was put on
 *	the rx queue. It is assumed that this routines runs at
 *	pri level splimp() ! Keep it short !
 *---------------------------------------------------------------------------*/
static void
ing_rx_data_rdy(int unit)
{
	register struct ing_softc *sc = &ing_softc[unit];
	register struct mbuf *m;
	int error;
	
	if((m = *isdn_linktab[unit]->rx_mbuf) == NULL)
		return;

#if I4BINGACCT
	sc->sc_inb += m->m_pkthdr.len;
#endif

	m->m_pkthdr.rcvif = NULL;

	sc->sc_inpkt++;
	
	NG_SEND_DATA_ONLY(error, sc->hook, m);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when the last frame has been sent out and there is no
 *	further frame (mbuf) in the tx queue.
 *---------------------------------------------------------------------------*/
static void
ing_tx_queue_empty(int unit)
{
	register struct ing_softc *sc = &ing_softc[unit];
	register struct mbuf *m;
	int x = 0;

	if(sc->sc_state != ST_CONNECTED)
		return;
		
	for(;;)
	{
		IF_DEQUEUE(&sc->xmitq_hipri, m);

		if(m == NULL)
		{
			IF_DEQUEUE(&sc->xmitq, m);
			if(m == NULL)
				break;
		}
	
#if I4BINGACCT
		sc->sc_outb += m->m_pkthdr.len;
#endif

		x = 1;

		if(! IF_HANDOFF(isdn_linktab[unit]->tx_queue, m, NULL))
		{
			NDBGL4(L4_INGDBG, "ing%d: tx queue full!", unit);
		}
	}

	if(x)
		(*isdn_linktab[unit]->bch_tx_start)(isdn_linktab[unit]->unit, isdn_linktab[unit]->channel);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	each time a packet is received or transmitted. It should
 *	be used to implement an activity timeout mechanism.
 *---------------------------------------------------------------------------*/
static void
ing_activity(int unit, int rxtx)
{
	ing_softc[unit].sc_cdp->last_active_time = SECOND;
}

/*---------------------------------------------------------------------------*
 *	return this drivers linktab address
 *---------------------------------------------------------------------------*/
drvr_link_t *
ing_ret_linktab(int unit)
{
	return(&ing_drvr_linktab[unit]);
}

/*---------------------------------------------------------------------------*
 *	setup the isdn_linktab for this driver
 *---------------------------------------------------------------------------*/
void
ing_set_linktab(int unit, isdn_link_t *ilt)
{
	isdn_linktab[unit] = ilt;
}

/*---------------------------------------------------------------------------*
 *	initialize this drivers linktab
 *---------------------------------------------------------------------------*/
static void
ing_init_linktab(int unit)
{
	ing_drvr_linktab[unit].unit = unit;
	ing_drvr_linktab[unit].bch_rx_data_ready = ing_rx_data_rdy;
	ing_drvr_linktab[unit].bch_tx_queue_empty = ing_tx_queue_empty;
	ing_drvr_linktab[unit].bch_activity = ing_activity;
	ing_drvr_linktab[unit].line_connected = ing_connect;
	ing_drvr_linktab[unit].line_disconnected = ing_disconnect;
	ing_drvr_linktab[unit].dial_response = ing_dialresponse;
	ing_drvr_linktab[unit].updown_ind = ing_updown;	
}

/*===========================================================================*
 *			NETGRAPH INTERFACE ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 * It is not possible or allowable to create a node of this type.
 * If the hardware exists, it will already have created it.
 *---------------------------------------------------------------------------*/
static int
ng_ing_constructor(node_p node)
{
	return(EINVAL);
}

/*---------------------------------------------------------------------------*
 * Give our ok for a hook to be added...
 * Add the hook's private info to the hook structure.
 *---------------------------------------------------------------------------*/
static int
ng_ing_newhook(node_p node, hook_p hook, const char *name)
{
	struct ing_softc *sc = NG_NODE_PRIVATE(node);

	/*
	 * check if it's our friend the debug hook
	 */
	if(strcmp(name, NG_ING_HOOK_DEBUG) == 0)
	{
		NG_HOOK_SET_PRIVATE(hook, NULL); /* paranoid */
		sc->debughook = hook;
		return (0);
	}
	/*
	 * Check for raw mode hook.
	 */
	if(strcmp(name, NG_ING_HOOK_RAW) == 0)
	{
		NG_HOOK_SET_PRIVATE(hook, sc);
		sc->hook = hook;
		return (0);
	}

	return (EINVAL);
}

/*---------------------------------------------------------------------------*
 * Get a netgraph control message.
 * Check it is one we understand. If needed, send a response.
 * We could save the address for an async action later, but don't here.
 * Always free the message.
 * The response should be in a malloc'd region that the caller can 'free'.
 * A response is not required.
 *---------------------------------------------------------------------------*/
static int
ng_ing_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	struct ing_softc *sc = NG_NODE_PRIVATE(node);

	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);

	if(msg->header.typecookie == NGM_GENERIC_COOKIE)
	{
		switch(msg->header.cmd)
		{
			case NGM_TEXT_STATUS:
			{
				char *arg;
				char *p;
				int pos = 0;

				NG_MKRESPONSE(resp, msg, sizeof(struct ng_mesg) + NG_TEXTRESPONSE, M_NOWAIT);

				if (resp == NULL)
				{
					error = ENOMEM;
					break;
				}
				arg = (char *) resp->data;

				switch(sc->sc_state)
				{
			    		case ST_IDLE:
						p = "idle";
						break;
				    	case ST_DIALING:
						p = "dialing";
						break;
				    	case ST_CONNECTED:
						p = "connected";
						break;
				    	default:
						p = "???";
						break;
			    	}

				pos = sprintf(arg, "state = %s (%d)\n", p, sc->sc_state);
#if I4BINGACCT
				pos += sprintf(arg + pos, "%d bytes in, %d bytes out\n", sc->sc_inb, sc->sc_outb);
#endif			    
				pos += sprintf(arg + pos, "%d pkts in, %d pkts out\n", sc->sc_inpkt, sc->sc_outpkt);

				resp->header.arglen = pos + 1;
				break;
			}

			default:
				error = EINVAL;
				break;
		}
	}
	else if(msg->header.typecookie == NGM_ING_COOKIE)
	{
		switch (msg->header.cmd)
		{
			case NGM_ING_GET_STATUS:
			{
				struct ngingstat *stats;

				NG_MKRESPONSE(resp, msg, sizeof(*stats), M_NOWAIT);

				if (!resp)
				{
					error = ENOMEM;
					break;
				}

				stats = (struct ngingstat *) resp->data;
				stats->packets_in = sc->packets_in;
				stats->packets_out = sc->packets_out;
				break;
			}

			case NGM_ING_SET_FLAG:
				if (msg->header.arglen != sizeof(u_int32_t))
				{
					error = EINVAL;
					break;
				}
				sc->flags = *((u_int32_t *) msg->data);
				break;

			default:
				error = EINVAL;		/* unknown command */
				break;
		}
	}
	else
	{
		error = EINVAL;			/* unknown cookie type */
	}

	/* Take care of synchronous response, if any */
	NG_RESPOND_MSG(error, node, item, resp);
	/* Free the message and return */
	NG_FREE_MSG(msg);
	return(error);
}

/*---------------------------------------------------------------------------*
 * get data from another node and transmit it out on a B-channel
 *---------------------------------------------------------------------------*/
static int
ng_ing_rcvdata(hook_p hook, item_p item)
{
	struct ing_softc *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ifqueue  *xmitq_p;
	int s;
	struct mbuf *m;
	struct ng_tag_prio *ptag;
	
	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	if(NG_HOOK_PRIVATE(hook) == NULL)
	{
		NG_FREE_M(m);
		return(ENETDOWN);
	}
	
	if(sc->sc_state == ST_IDLE || sc->sc_state == ST_DIALING)
	{
		i4b_l4_dialout(BDRV_ING, sc->sc_unit);
		sc->sc_state = ST_DIALING;
	}

	sc->sc_outpkt++;
	
       /*
	* Now queue the data for when it can be sent
	*/
	if ((ptag = (struct ng_tag_prio *)m_tag_locate(m, NGM_GENERIC_COOKIE,
	    NG_TAG_PRIO, NULL)) != NULL && (ptag->priority > NG_PRIO_CUTOFF) )
		xmitq_p = (&sc->xmitq_hipri);
	else
		xmitq_p = (&sc->xmitq);

	s = splimp();

	IF_LOCK(xmitq_p);
	if (_IF_QFULL(xmitq_p))
	{
		_IF_DROP(xmitq_p);
		IF_UNLOCK(xmitq_p);
		splx(s);
		NG_FREE_M(m);
		return(ENOBUFS);
	}

	_IF_ENQUEUE(xmitq_p, m);
	IF_UNLOCK(xmitq_p);

	ing_tx_queue_empty(sc->sc_unit);

	splx(s);
	return (0);
}

/*---------------------------------------------------------------------------*
 * Do local shutdown processing..
 * If we are a persistant device, we might refuse to go away, and
 * we'd only remove our links and reset ourself.
 *---------------------------------------------------------------------------*/
static int
ng_ing_shutdown(node_p node)
{
	struct ing_softc *sc = NG_NODE_PRIVATE(node);
	int	ret;

	NG_NODE_UNREF(node);

	sc->packets_in = 0;		/* reset stats */
	sc->packets_out = 0;

	if ((ret = ng_make_node_common(&typestruct, &sc->node)))
	{
		printf("ing: ng_make_node_common, ret = %d\n!", ret);
	}

	/* name the netgraph node */
	sprintf(sc->nodename, "%s%d", NG_ING_NODE_TYPE, sc->sc_unit);
	if((ret = ng_name_node(sc->node, sc->nodename)))
	{
		printf("ing: ng_name node, ret = %d\n!", ret);
		NG_NODE_UNREF(sc->node);
		return (0);
	}

	NG_NODE_SET_PRIVATE(sc->node, sc);

	return (0);
}

/*---------------------------------------------------------------------------*
 * This is called once we've already connected a new hook to the other node.
 *---------------------------------------------------------------------------*/
static int
ng_ing_connect(hook_p hook)
{
	/* probably not at splnet, force outward queueing */
	NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));
	return (0);
}

/*
 * Dook disconnection
 *
 * For this type, removal of the last link destroys the node
 */
static int
ng_ing_disconnect(hook_p hook)
{
	struct ing_softc *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int s;
	
	if(NG_HOOK_PRIVATE(hook))
	{
		s = splimp();
		splx(s);
	}
	else
	{
		sc->debughook = NULL;
	}
	return (0);
}

/*===========================================================================*/
