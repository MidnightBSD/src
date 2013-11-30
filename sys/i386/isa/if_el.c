/*-
 * Copyright (c) 1994, Matthew E. Kimmel.  Permission is hereby granted
 * to use, copy, modify and distribute this software provided that both
 * the copyright notice and this permission notice appear in all copies
 * of the software, derivative works or modified versions, and any
 * portions thereof.
 *
 * Questions, comments, bug reports and fixes to kimmel@cs.umass.edu.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/isa/if_el.c,v 1.67.2.1 2005/08/25 05:01:18 rwatson Exp $");

/* Except of course for the portions of code lifted from other FreeBSD
 * drivers (mainly elread, elget and el_ioctl)
 */
/* 3COM Etherlink 3C501 device driver for FreeBSD */
/* Yeah, I know these cards suck, but you can also get them for free
 * really easily...
 */
/* Bugs/possible improvements:
 *	- Does not currently support DMA
 *	- Does not currently support multicasts
 */
#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <i386/isa/if_elreg.h>

/* For debugging convenience */
#ifdef EL_DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

/* el_softc: per line info and status */
struct el_softc {
	struct ifnet		*el_ifp;
	u_char			el_enaddr[6];
	bus_space_handle_t	el_bhandle;
	bus_space_tag_t		el_btag;
	void			*el_intrhand;
	struct resource		*el_irq;
	struct resource		*el_res;
	struct mtx		el_mtx;
	char el_pktbuf[EL_BUFSIZ]; 	/* Frame buffer */
};

/* Prototypes */
static int el_attach(device_t);
static int el_detach(device_t);
static void el_init(void *);
static int el_ioctl(struct ifnet *,u_long,caddr_t);
static int el_probe(device_t);
static void el_start(struct ifnet *);
static void el_reset(void *);
static void el_watchdog(struct ifnet *);
static int el_shutdown(device_t);

static void el_stop(void *);
static int el_xmit(struct el_softc *,int);
static void elintr(void *);
static __inline void elread(struct el_softc *,caddr_t,int);
static struct mbuf *elget(caddr_t,int,struct ifnet *);
static __inline void el_hardreset(void *);

static device_method_t el_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,		el_probe),
	DEVMETHOD(device_attach,	el_attach),
	DEVMETHOD(device_detach,	el_detach),
	DEVMETHOD(device_shutdown,	el_shutdown),
	{ 0, 0 }
};

static driver_t el_driver = {
	"el",
	el_methods,
	sizeof(struct el_softc)
};

static devclass_t el_devclass;

DRIVER_MODULE(if_el, isa, el_driver, el_devclass, 0, 0);

#define CSR_WRITE_1(sc, reg, val)       \
	bus_space_write_1(sc->el_btag, sc->el_bhandle, reg, val)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->el_btag, sc->el_bhandle, reg)

#define EL_LOCK(_sc)		mtx_lock(&(_sc)->el_mtx)
#define EL_UNLOCK(_sc)		mtx_unlock(&(_sc)->el_mtx)

/* Probe routine.  See if the card is there and at the right place. */
static int
el_probe(device_t dev)
{
	struct el_softc *sc;
	u_short base; /* Just for convenience */
	int i, rid;

	/* Grab some info for our structure */
	sc = device_get_softc(dev);

	if (isa_get_logicalid(dev))		/* skip PnP probes */
		return (ENXIO);

	if ((base = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0)
		return (ENXIO);

	/* First check the base */
	if((base < 0x280) || (base > 0x3f0)) {
		device_printf(dev,
		    "ioaddr must be between 0x280 and 0x3f0\n");
		return(ENXIO);
	}

	/* Temporarily map the resources. */
	rid = 0;
	sc->el_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
	    0, ~0, EL_IOSIZ, RF_ACTIVE);

	if (sc->el_res == NULL)
		return(ENXIO);

	sc->el_btag = rman_get_bustag(sc->el_res);
	sc->el_bhandle = rman_get_bushandle(sc->el_res);
	mtx_init(&sc->el_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	EL_LOCK(sc);

	/* Now attempt to grab the station address from the PROM
	 * and see if it contains the 3com vendor code.
	 */
	dprintf(("Probing 3c501 at 0x%x...\n",base));

	/* Reset the board */
	dprintf(("Resetting board...\n"));
	CSR_WRITE_1(sc,EL_AC,EL_AC_RESET);
	DELAY(5);
	CSR_WRITE_1(sc,EL_AC,0);
	dprintf(("Reading station address...\n"));
	/* Now read the address */
	for(i=0;i<ETHER_ADDR_LEN;i++) {
		CSR_WRITE_1(sc,EL_GPBL,i);
		sc->el_enaddr[i] = CSR_READ_1(sc,EL_EAW);
	}

	/* Now release resources */
	bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->el_res);
	EL_UNLOCK(sc);
	mtx_destroy(&sc->el_mtx);

	dprintf(("Address is %6D\n",sc->el_enaddr, ":"));

	/* If the vendor code is ok, return a 1.  We'll assume that
	 * whoever configured this system is right about the IRQ.
	 */
	if((sc->el_enaddr[0] != 0x02) || (sc->el_enaddr[1] != 0x60)
	   || (sc->el_enaddr[2] != 0x8c)) {
		dprintf(("Bad vendor code.\n"));
		return(ENXIO);
	} else {
		dprintf(("Vendor code ok.\n"));
	}

	device_set_desc(dev, "3Com 3c501 Ethernet");

	return(0);
}

/* Do a hardware reset of the 3c501.  Do not call until after el_probe()! */
static __inline void
el_hardreset(xsc)
	void *xsc;
{
	register struct el_softc *sc = xsc;
	register int j;

	/* First reset the board */
	CSR_WRITE_1(sc,EL_AC,EL_AC_RESET);
	DELAY(5);
	CSR_WRITE_1(sc,EL_AC,0);

	/* Then give it back its ethernet address.  Thanks to the mach
	 * source code for this undocumented goodie...
	 */
	for(j=0;j<ETHER_ADDR_LEN;j++)
		CSR_WRITE_1(sc,j,IFP2ENADDR(sc->el_ifp)[j]);
}

/* Attach the interface to the kernel data structures.  By the time
 * this is called, we know that the card exists at the given I/O address.
 * We still assume that the IRQ given is correct.
 */
static int
el_attach(device_t dev)
{
	struct el_softc *sc;
	struct ifnet *ifp;
	int rid, error;

	dprintf(("Attaching el%d...\n",device_get_unit(dev)));

	/* Get things pointing to the right places. */
	sc = device_get_softc(dev);
	ifp = sc->el_ifp = if_alloc(IFT_ETHER);

	if (ifp == NULL)
		return (ENOSPC);

	rid = 0;
	sc->el_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
	    0, ~0, EL_IOSIZ, RF_ACTIVE);

	if (sc->el_res == NULL) {
		if_free(ifp);
		return(ENXIO);
	}

	rid = 0;
	sc->el_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
            RF_SHAREABLE | RF_ACTIVE);

        if (sc->el_irq == NULL) {
		if_free(ifp);
                bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->el_res);
		return(ENXIO);
	}

	error = bus_setup_intr(dev, sc->el_irq, INTR_TYPE_NET,
		elintr, sc, &sc->el_intrhand);

	if (error) {
		if_free(ifp);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->el_irq);
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->el_res);
		return(ENXIO);
	}

	/* Initialize ifnet structure */
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_start = el_start;
	ifp->if_ioctl = el_ioctl;
	ifp->if_watchdog = el_watchdog;
	ifp->if_init = el_init;
	ifp->if_flags = (IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX);
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	/* Now we can attach the interface */
	dprintf(("Attaching interface...\n"));
	ether_ifattach(ifp, sc->el_enaddr);

	/* Now reset the board */
	dprintf(("Resetting board...\n"));
	el_hardreset(sc);

	dprintf(("el_attach() finished.\n"));
	return(0);
}

static int el_detach(dev)
	device_t dev;
{
	struct el_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->el_ifp;

	el_stop(sc);
	EL_LOCK(sc);
	ether_ifdetach(ifp);
	if_free(ifp);
	bus_teardown_intr(dev, sc->el_irq, sc->el_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->el_irq);
	bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->el_res);
	EL_UNLOCK(sc);
	mtx_destroy(&sc->el_mtx);

	return(0);
}

static int
el_shutdown(dev)
	device_t dev;
{
	struct el_softc *sc;

	sc = device_get_softc(dev);
	el_stop(sc);
	return(0);
}

/* This routine resets the interface. */
static void 
el_reset(xsc)
	void *xsc;
{
	struct el_softc *sc = xsc;

	dprintf(("elreset()\n"));
	el_stop(sc);
	el_init(sc);
}

static void el_stop(xsc)
	void *xsc;
{
	struct el_softc *sc = xsc;

	EL_LOCK(sc);
	CSR_WRITE_1(sc,EL_AC,0);
	el_hardreset(sc);
	EL_UNLOCK(sc);
}

/* Initialize interface.  */
static void 
el_init(xsc)
	void *xsc;
{
	struct el_softc *sc = xsc;
	struct ifnet *ifp;

	/* Set up pointers */
	ifp = sc->el_ifp;

	/* If address not known, do nothing. */
	if(TAILQ_EMPTY(&ifp->if_addrhead)) /* XXX unlikely */
		return;

	EL_LOCK(sc);

	/* First, reset the board. */
	dprintf(("Resetting board...\n"));
	el_hardreset(sc);

	/* Configure rx */
	dprintf(("Configuring rx...\n"));
	if(ifp->if_flags & IFF_PROMISC)
		CSR_WRITE_1(sc,EL_RXC,
		    (EL_RXC_PROMISC|EL_RXC_ABROAD|EL_RXC_AMULTI|
		    EL_RXC_AGF|EL_RXC_DSHORT|EL_RXC_DDRIB|EL_RXC_DOFLOW));
	else
		CSR_WRITE_1(sc,EL_RXC,
		    (EL_RXC_ABROAD|EL_RXC_AMULTI|
		    EL_RXC_AGF|EL_RXC_DSHORT|EL_RXC_DDRIB|EL_RXC_DOFLOW));
	CSR_WRITE_1(sc,EL_RBC,0);

	/* Configure TX */
	dprintf(("Configuring tx...\n"));
	CSR_WRITE_1(sc,EL_TXC,0);

	/* Start reception */
	dprintf(("Starting reception...\n"));
	CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));

	/* Set flags appropriately */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	/* And start output. */
	el_start(ifp);

	EL_UNLOCK(sc);
}

/* Start output on interface.  Get datagrams from the queue and output
 * them, giving the receiver a chance between datagrams.  Call only
 * from splimp or interrupt level!
 */
static void
el_start(struct ifnet *ifp)
{
	struct el_softc *sc;
	struct mbuf *m, *m0;
	int i, len, retries, done;

	/* Get things pointing in the right directions */
	sc = ifp->if_softc;

	dprintf(("el_start()...\n"));
	EL_LOCK(sc);

	/* Don't do anything if output is active */
	if(sc->el_ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;
	sc->el_ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	/* The main loop.  They warned me against endless loops, but
	 * would I listen?  NOOO....
	 */
	while(1) {
		/* Dequeue the next datagram */
		IF_DEQUEUE(&sc->el_ifp->if_snd,m0);

		/* If there's nothing to send, return. */
		if(m0 == NULL) {
			sc->el_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
			EL_UNLOCK(sc);
			return;
		}

		/* Disable the receiver */
		CSR_WRITE_1(sc,EL_AC,EL_AC_HOST);
		CSR_WRITE_1(sc,EL_RBC,0);

		/* Copy the datagram to the buffer. */
		len = 0;
		for(m = m0; m != NULL; m = m->m_next) {
			if(m->m_len == 0)
				continue;
			bcopy(mtod(m,caddr_t),sc->el_pktbuf+len,m->m_len);
			len += m->m_len;
		}
		m_freem(m0);

		len = max(len,ETHER_MIN_LEN);

		/* Give the packet to the bpf, if any */
		BPF_TAP(sc->el_ifp, sc->el_pktbuf, len);

		/* Transfer datagram to board */
		dprintf(("el: xfr pkt length=%d...\n",len));
		i = EL_BUFSIZ - len;
		CSR_WRITE_1(sc,EL_GPBL,(i & 0xff));
		CSR_WRITE_1(sc,EL_GPBH,((i>>8)&0xff));
		bus_space_write_multi_1(sc->el_btag, sc->el_bhandle,
		    EL_BUF, sc->el_pktbuf, len);

		/* Now transmit the datagram */
		retries=0;
		done=0;
		while(!done) {
			if(el_xmit(sc,len)) { /* Something went wrong */
				done = -1;
				break;
			}
			/* Check out status */
			i = CSR_READ_1(sc,EL_TXS);
			dprintf(("tx status=0x%x\n",i));
			if(!(i & EL_TXS_READY)) {
				dprintf(("el: err txs=%x\n",i));
				sc->el_ifp->if_oerrors++;
				if(i & (EL_TXS_COLL|EL_TXS_COLL16)) {
					if((!(i & EL_TXC_DCOLL16)) && retries < 15) {
						retries++;
						CSR_WRITE_1(sc,EL_AC,EL_AC_HOST);
					}
				}
				else
					done = 1;
			}
			else {
				sc->el_ifp->if_opackets++;
				done = 1;
			}
		}
		if(done == -1)  /* Packet not transmitted */
			continue;

		/* Now give the card a chance to receive.
		 * Gotta love 3c501s...
		 */
		(void)CSR_READ_1(sc,EL_AS);
		CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));
		EL_UNLOCK(sc);
		/* Interrupt here */
		EL_LOCK(sc);
	}
}

/* This function actually attempts to transmit a datagram downloaded
 * to the board.  Call at splimp or interrupt, after downloading data!
 * Returns 0 on success, non-0 on failure
 */
static int
el_xmit(struct el_softc *sc,int len)
{
	int gpl;
	int i;

	gpl = EL_BUFSIZ - len;
	dprintf(("el: xmit..."));
	CSR_WRITE_1(sc,EL_GPBL,(gpl & 0xff));
	CSR_WRITE_1(sc,EL_GPBH,((gpl>>8)&0xff));
	CSR_WRITE_1(sc,EL_AC,EL_AC_TXFRX);
	i = 20000;
	while((CSR_READ_1(sc,EL_AS) & EL_AS_TXBUSY) && (i>0))
		i--;
	if(i == 0) {
		dprintf(("tx not ready\n"));
		sc->el_ifp->if_oerrors++;
		return(-1);
	}
	dprintf(("%d cycles.\n",(20000-i)));
	return(0);
}

/* Pass a packet up to the higher levels. */
static __inline void
elread(struct el_softc *sc,caddr_t buf,int len)
{
	struct ifnet *ifp = sc->el_ifp;
	struct mbuf *m;

	/*
	 * Put packet into an mbuf chain
	 */
	m = elget(buf,len,ifp);
	if(m == 0)
		return;

	(*ifp->if_input)(ifp,m);
}

/* controller interrupt */
static void
elintr(void *xsc)
{
	register struct el_softc *sc;
	int stat, rxstat, len, done;


	/* Get things pointing properly */
	sc = xsc;
	EL_LOCK(sc);
	dprintf(("elintr: "));

	/* Check board status */
	stat = CSR_READ_1(sc,EL_AS);
	if(stat & EL_AS_RXBUSY) {
		(void)CSR_READ_1(sc,EL_RXC);
		CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));
		EL_UNLOCK(sc);
		return;
	}

	done = 0;
	while(!done) {
		rxstat = CSR_READ_1(sc,EL_RXS);
		if(rxstat & EL_RXS_STALE) {
			(void)CSR_READ_1(sc,EL_RXC);
			CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));
			EL_UNLOCK(sc);
			return;
		}

		/* If there's an overflow, reinit the board. */
		if(!(rxstat & EL_RXS_NOFLOW)) {
			dprintf(("overflow.\n"));
			el_hardreset(sc);
			/* Put board back into receive mode */
			if(sc->el_ifp->if_flags & IFF_PROMISC)
				CSR_WRITE_1(sc,EL_RXC,
				    (EL_RXC_PROMISC|EL_RXC_ABROAD|
		    		    EL_RXC_AMULTI|EL_RXC_AGF|EL_RXC_DSHORT|
				    EL_RXC_DDRIB|EL_RXC_DOFLOW));
			else
				CSR_WRITE_1(sc,EL_RXC,
				    (EL_RXC_ABROAD|
		    		    EL_RXC_AMULTI|EL_RXC_AGF|EL_RXC_DSHORT|
				    EL_RXC_DDRIB|EL_RXC_DOFLOW));
			(void)CSR_READ_1(sc,EL_AS);
			CSR_WRITE_1(sc,EL_RBC,0);
			(void)CSR_READ_1(sc,EL_RXC);
			CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));
			EL_UNLOCK(sc);
			return;
		}

		/* Incoming packet */
		len = CSR_READ_1(sc,EL_RBL);
		len |= CSR_READ_1(sc,EL_RBH) << 8;
		dprintf(("receive len=%d rxstat=%x ",len,rxstat));
		CSR_WRITE_1(sc,EL_AC,EL_AC_HOST);

		/* If packet too short or too long, restore rx mode and return
		 */
		if((len <= sizeof(struct ether_header)) || (len > ETHER_MAX_LEN)) {
			if(sc->el_ifp->if_flags & IFF_PROMISC)
				CSR_WRITE_1(sc,EL_RXC,
				    (EL_RXC_PROMISC|EL_RXC_ABROAD|
		    		    EL_RXC_AMULTI|EL_RXC_AGF|EL_RXC_DSHORT|
				    EL_RXC_DDRIB|EL_RXC_DOFLOW));
			else
				CSR_WRITE_1(sc,EL_RXC,
				    (EL_RXC_ABROAD|
		    		    EL_RXC_AMULTI|EL_RXC_AGF|EL_RXC_DSHORT|
				    EL_RXC_DDRIB|EL_RXC_DOFLOW));
			(void)CSR_READ_1(sc,EL_AS);
			CSR_WRITE_1(sc,EL_RBC,0);
			(void)CSR_READ_1(sc,EL_RXC);
			CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));
			EL_UNLOCK(sc);
			return;
		}

		sc->el_ifp->if_ipackets++;

		/* Copy the data into our buffer */
		CSR_WRITE_1(sc,EL_GPBL,0);
		CSR_WRITE_1(sc,EL_GPBH,0);
		bus_space_read_multi_1(sc->el_btag, sc->el_bhandle,
		    EL_BUF, sc->el_pktbuf, len);
		CSR_WRITE_1(sc,EL_RBC,0);
		CSR_WRITE_1(sc,EL_AC,EL_AC_RX);
		dprintf(("%6D-->",sc->el_pktbuf+6,":"));
		dprintf(("%6D\n",sc->el_pktbuf,":"));

		/* Pass data up to upper levels */
		elread(sc,(caddr_t)(sc->el_pktbuf),len);

		/* Is there another packet? */
		stat = CSR_READ_1(sc,EL_AS);

		/* If so, do it all again (i.e. don't set done to 1) */
		if(!(stat & EL_AS_RXBUSY))
			dprintf(("<rescan> "));
		else
			done = 1;
	}

	(void)CSR_READ_1(sc,EL_RXC);
	CSR_WRITE_1(sc,EL_AC,(EL_AC_IRQE|EL_AC_RX));
	EL_UNLOCK(sc);
	return;
}

/*
 * Pull read data off an interface.
 * Len is length of data, with local net header stripped.
 */
static struct mbuf *
elget(buf, totlen, ifp)
        caddr_t buf;
        int totlen;
        struct ifnet *ifp;
{
        struct mbuf *top, **mp, *m;
        int len;
        register caddr_t cp;
        char *epkt;

        buf += sizeof(struct ether_header);
        cp = buf;
        epkt = cp + totlen;

        MGETHDR(m, M_DONTWAIT, MT_DATA);
        if (m == 0)
                return (0);
        m->m_pkthdr.rcvif = ifp;
        m->m_pkthdr.len = totlen;
        m->m_len = MHLEN;
        top = 0;
        mp = &top;
        while (totlen > 0) {
                if (top) {
                        MGET(m, M_DONTWAIT, MT_DATA);
                        if (m == 0) {
                                m_freem(top);
                                return (0);
                        }
                        m->m_len = MLEN;
                }
                len = min(totlen, epkt - cp);
                if (len >= MINCLSIZE) {
                        MCLGET(m, M_DONTWAIT);
                        if (m->m_flags & M_EXT)
                                m->m_len = len = min(len, MCLBYTES);
                        else
                                len = m->m_len;
                } else {
                        /*
                         * Place initial small packet/header at end of mbuf.
                         */
                        if (len < m->m_len) {
                                if (top == 0 && len + max_linkhdr <= m->m_len)
                                        m->m_data += max_linkhdr;
                                m->m_len = len;
                        } else
                                len = m->m_len;
                }
                bcopy(cp, mtod(m, caddr_t), (unsigned)len);
                cp += len;
                *mp = m;
                mp = &m->m_next;
                totlen -= len;
                if (cp == epkt)
                        cp = buf;
        }
        return (top);
}

/*
 * Process an ioctl request. This code needs some work - it looks
 *	pretty ugly.
 */
static int
el_ioctl(ifp, command, data)
	register struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	int error = 0;
	struct el_softc *sc;

	sc = ifp->if_softc;
	EL_LOCK(sc);

	switch (command) {
	case SIOCSIFFLAGS:
		/*
		 * If interface is marked down and it is running, then stop it
		 */
		if (((ifp->if_flags & IFF_UP) == 0) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			el_stop(ifp->if_softc);
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		} else {
		/*
		 * If interface is marked up and it is stopped, then start it
		 */
			if ((ifp->if_flags & IFF_UP) &&
		    	    ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0))
				el_init(ifp->if_softc);
		}
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	EL_UNLOCK(sc);
	return (error);
}

/* Device timeout routine */
static void
el_watchdog(struct ifnet *ifp)
{
	log(LOG_ERR,"%s: device timeout\n", ifp->if_xname);
	ifp->if_oerrors++;
	el_reset(ifp->if_softc);
}
