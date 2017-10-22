/*-
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2000, 2001
 *	Bill Paul <wpaul@bsdi.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/nge/if_nge.c 167190 2007-03-04 03:38:08Z csjp $");

/*
 * National Semiconductor DP83820/DP83821 gigabit ethernet driver
 * for FreeBSD. Datasheets are available from:
 *
 * http://www.national.com/ds/DP/DP83820.pdf
 * http://www.national.com/ds/DP/DP83821.pdf
 *
 * These chips are used on several low cost gigabit ethernet NICs
 * sold by D-Link, Addtron, SMC and Asante. Both parts are
 * virtually the same, except the 83820 is a 64-bit/32-bit part,
 * while the 83821 is 32-bit only.
 *
 * Many cards also use National gigE transceivers, such as the
 * DP83891, DP83861 and DP83862 gigPHYTER parts. The DP83861 datasheet
 * contains a full register description that applies to all of these
 * components:
 *
 * http://www.national.com/ds/DP/DP83861.pdf
 *
 * Written by Bill Paul <wpaul@bsdi.com>
 * BSDi Open Source Solutions
 */

/*
 * The NatSemi DP83820 and 83821 controllers are enhanced versions
 * of the NatSemi MacPHYTER 10/100 devices. They support 10, 100
 * and 1000Mbps speeds with 1000baseX (ten bit interface), MII and GMII
 * ports. Other features include 8K TX FIFO and 32K RX FIFO, TCP/IP
 * hardware checksum offload (IPv4 only), VLAN tagging and filtering,
 * priority TX and RX queues, a 2048 bit multicast hash filter, 4 RX pattern
 * matching buffers, one perfect address filter buffer and interrupt
 * moderation. The 83820 supports both 64-bit and 32-bit addressing
 * and data transfers: the 64-bit support can be toggled on or off
 * via software. This affects the size of certain fields in the DMA
 * descriptors.
 *
 * There are two bugs/misfeatures in the 83820/83821 that I have
 * discovered so far:
 *
 * - Receive buffers must be aligned on 64-bit boundaries, which means
 *   you must resort to copying data in order to fix up the payload
 *   alignment.
 *
 * - In order to transmit jumbo frames larger than 8170 bytes, you have
 *   to turn off transmit checksum offloading, because the chip can't
 *   compute the checksum on an outgoing frame unless it fits entirely
 *   within the TX FIFO, which is only 8192 bytes in size. If you have
 *   TX checksum offload enabled and you transmit attempt to transmit a
 *   frame larger than 8170 bytes, the transmitter will wedge.
 *
 * To work around the latter problem, TX checksum offload is disabled
 * if the user selects an MTU larger than 8152 (8170 - 18).
 */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define NGE_USEIOSPACE

#include <dev/nge/if_ngereg.h>

MODULE_DEPEND(nge, pci, 1, 1, 1);
MODULE_DEPEND(nge, ether, 1, 1, 1);
MODULE_DEPEND(nge, miibus, 1, 1, 1);

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#define NGE_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)

/*
 * Various supported device vendors/types and their names.
 */
static struct nge_type nge_devs[] = {
	{ NGE_VENDORID, NGE_DEVICEID,
	    "National Semiconductor Gigabit Ethernet" },
	{ 0, 0, NULL }
};

static int nge_probe(device_t);
static int nge_attach(device_t);
static int nge_detach(device_t);

static int nge_newbuf(struct nge_softc *, struct nge_desc *, struct mbuf *);
static int nge_encap(struct nge_softc *, struct mbuf *, u_int32_t *);
#ifdef NGE_FIXUP_RX
static __inline void nge_fixup_rx (struct mbuf *);
#endif
static void nge_rxeof(struct nge_softc *);
static void nge_txeof(struct nge_softc *);
static void nge_intr(void *);
static void nge_tick(void *);
static void nge_start(struct ifnet *);
static void nge_start_locked(struct ifnet *);
static int nge_ioctl(struct ifnet *, u_long, caddr_t);
static void nge_init(void *);
static void nge_init_locked(struct nge_softc *);
static void nge_stop(struct nge_softc *);
static void nge_watchdog(struct ifnet *);
static void nge_shutdown(device_t);
static int nge_ifmedia_upd(struct ifnet *);
static void nge_ifmedia_upd_locked(struct ifnet *);
static void nge_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void nge_delay(struct nge_softc *);
static void nge_eeprom_idle(struct nge_softc *);
static void nge_eeprom_putbyte(struct nge_softc *, int);
static void nge_eeprom_getword(struct nge_softc *, int, u_int16_t *);
static void nge_read_eeprom(struct nge_softc *, caddr_t, int, int, int);

static void nge_mii_sync(struct nge_softc *);
static void nge_mii_send(struct nge_softc *, u_int32_t, int);
static int nge_mii_readreg(struct nge_softc *, struct nge_mii_frame *);
static int nge_mii_writereg(struct nge_softc *, struct nge_mii_frame *);

static int nge_miibus_readreg(device_t, int, int);
static int nge_miibus_writereg(device_t, int, int, int);
static void nge_miibus_statchg(device_t);

static void nge_setmulti(struct nge_softc *);
static void nge_reset(struct nge_softc *);
static int nge_list_rx_init(struct nge_softc *);
static int nge_list_tx_init(struct nge_softc *);

#ifdef NGE_USEIOSPACE
#define NGE_RES			SYS_RES_IOPORT
#define NGE_RID			NGE_PCI_LOIO
#else
#define NGE_RES			SYS_RES_MEMORY
#define NGE_RID			NGE_PCI_LOMEM
#endif

static device_method_t nge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nge_probe),
	DEVMETHOD(device_attach,	nge_attach),
	DEVMETHOD(device_detach,	nge_detach),
	DEVMETHOD(device_shutdown,	nge_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	nge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	nge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	nge_miibus_statchg),

	{ 0, 0 }
};

static driver_t nge_driver = {
	"nge",
	nge_methods,
	sizeof(struct nge_softc)
};

static devclass_t nge_devclass;

DRIVER_MODULE(nge, pci, nge_driver, nge_devclass, 0, 0);
DRIVER_MODULE(miibus, nge, miibus_driver, miibus_devclass, 0, 0);

#define NGE_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | (x))

#define NGE_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, NGE_MEAR, CSR_READ_4(sc, NGE_MEAR) | (x))

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, NGE_MEAR, CSR_READ_4(sc, NGE_MEAR) & ~(x))

static void
nge_delay(sc)
	struct nge_softc	*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, NGE_CSR);

	return;
}

static void
nge_eeprom_idle(sc)
	struct nge_softc	*sc;
{
	register int		i;

	SIO_SET(NGE_MEAR_EE_CSEL);
	nge_delay(sc);
	SIO_SET(NGE_MEAR_EE_CLK);
	nge_delay(sc);

	for (i = 0; i < 25; i++) {
		SIO_CLR(NGE_MEAR_EE_CLK);
		nge_delay(sc);
		SIO_SET(NGE_MEAR_EE_CLK);
		nge_delay(sc);
	}

	SIO_CLR(NGE_MEAR_EE_CLK);
	nge_delay(sc);
	SIO_CLR(NGE_MEAR_EE_CSEL);
	nge_delay(sc);
	CSR_WRITE_4(sc, NGE_MEAR, 0x00000000);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
nge_eeprom_putbyte(sc, addr)
	struct nge_softc	*sc;
	int			addr;
{
	register int		d, i;

	d = addr | NGE_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(NGE_MEAR_EE_DIN);
		} else {
			SIO_CLR(NGE_MEAR_EE_DIN);
		}
		nge_delay(sc);
		SIO_SET(NGE_MEAR_EE_CLK);
		nge_delay(sc);
		SIO_CLR(NGE_MEAR_EE_CLK);
		nge_delay(sc);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
nge_eeprom_getword(sc, addr, dest)
	struct nge_softc	*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	nge_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	nge_delay(sc);
	SIO_CLR(NGE_MEAR_EE_CLK);
	nge_delay(sc);
	SIO_SET(NGE_MEAR_EE_CSEL);
	nge_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	nge_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(NGE_MEAR_EE_CLK);
		nge_delay(sc);
		if (CSR_READ_4(sc, NGE_MEAR) & NGE_MEAR_EE_DOUT)
			word |= i;
		nge_delay(sc);
		SIO_CLR(NGE_MEAR_EE_CLK);
		nge_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	nge_eeprom_idle(sc);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
nge_read_eeprom(sc, dest, off, cnt, swap)
	struct nge_softc	*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		nge_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
nge_mii_sync(sc)
	struct nge_softc		*sc;
{
	register int		i;

	SIO_SET(NGE_MEAR_MII_DIR|NGE_MEAR_MII_DATA);

	for (i = 0; i < 32; i++) {
		SIO_SET(NGE_MEAR_MII_CLK);
		DELAY(1);
		SIO_CLR(NGE_MEAR_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void
nge_mii_send(sc, bits, cnt)
	struct nge_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	SIO_CLR(NGE_MEAR_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			SIO_SET(NGE_MEAR_MII_DATA);
                } else {
			SIO_CLR(NGE_MEAR_MII_DATA);
                }
		DELAY(1);
		SIO_CLR(NGE_MEAR_MII_CLK);
		DELAY(1);
		SIO_SET(NGE_MEAR_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int
nge_mii_readreg(sc, frame)
	struct nge_softc		*sc;
	struct nge_mii_frame	*frame;
	
{
	int			i, ack;

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = NGE_MII_STARTDELIM;
	frame->mii_opcode = NGE_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_4(sc, NGE_MEAR, 0);

	/*
 	 * Turn on data xmit.
	 */
	SIO_SET(NGE_MEAR_MII_DIR);

	nge_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	nge_mii_send(sc, frame->mii_stdelim, 2);
	nge_mii_send(sc, frame->mii_opcode, 2);
	nge_mii_send(sc, frame->mii_phyaddr, 5);
	nge_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	SIO_CLR((NGE_MEAR_MII_CLK|NGE_MEAR_MII_DATA));
	DELAY(1);
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(NGE_MEAR_MII_DIR);
	/* Check for ack */
	SIO_CLR(NGE_MEAR_MII_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, NGE_MEAR) & NGE_MEAR_MII_DATA;
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(NGE_MEAR_MII_CLK);
			DELAY(1);
			SIO_SET(NGE_MEAR_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(NGE_MEAR_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, NGE_MEAR) & NGE_MEAR_MII_DATA)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(NGE_MEAR_MII_CLK);
		DELAY(1);
	}

fail:

	SIO_CLR(NGE_MEAR_MII_CLK);
	DELAY(1);
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
nge_mii_writereg(sc, frame)
	struct nge_softc		*sc;
	struct nge_mii_frame	*frame;
	
{

	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = NGE_MII_STARTDELIM;
	frame->mii_opcode = NGE_MII_WRITEOP;
	frame->mii_turnaround = NGE_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	SIO_SET(NGE_MEAR_MII_DIR);

	nge_mii_sync(sc);

	nge_mii_send(sc, frame->mii_stdelim, 2);
	nge_mii_send(sc, frame->mii_opcode, 2);
	nge_mii_send(sc, frame->mii_phyaddr, 5);
	nge_mii_send(sc, frame->mii_regaddr, 5);
	nge_mii_send(sc, frame->mii_turnaround, 2);
	nge_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);
	SIO_CLR(NGE_MEAR_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	SIO_CLR(NGE_MEAR_MII_DIR);

	return(0);
}

static int
nge_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct nge_softc	*sc;
	struct nge_mii_frame	frame;

	sc = device_get_softc(dev);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	nge_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static int
nge_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct nge_softc	*sc;
	struct nge_mii_frame	frame;

	sc = device_get_softc(dev);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;
	nge_mii_writereg(sc, &frame);

	return(0);
}

static void
nge_miibus_statchg(dev)
	device_t		dev;
{
	int			status;	
	struct nge_softc	*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	if (sc->nge_tbi) {
		if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media)
		    == IFM_AUTO) {
			status = CSR_READ_4(sc, NGE_TBI_ANLPAR);
			if (status == 0 || status & NGE_TBIANAR_FDX) {
				NGE_SETBIT(sc, NGE_TX_CFG,
				    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
				NGE_SETBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
			} else {
				NGE_CLRBIT(sc, NGE_TX_CFG,
				    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
				NGE_CLRBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
			}

		} else if ((sc->nge_ifmedia.ifm_cur->ifm_media & IFM_GMASK) 
			!= IFM_FDX) {
			NGE_CLRBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_CLRBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		} else {
			NGE_SETBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_SETBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		}
	} else {
		mii = device_get_softc(sc->nge_miibus);

		if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		        NGE_SETBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_SETBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		} else {
			NGE_CLRBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_CLRBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		}

		/* If we have a 1000Mbps link, set the mode_1000 bit. */
		if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T ||
		    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX) {
			NGE_SETBIT(sc, NGE_CFG, NGE_CFG_MODE_1000);
		} else {
			NGE_CLRBIT(sc, NGE_CFG, NGE_CFG_MODE_1000);
		}
	}
	return;
}

static void
nge_setmulti(sc)
	struct nge_softc	*sc;
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i, filtsave;
	int			bit, index;

	NGE_LOCK_ASSERT(sc);
	ifp = sc->nge_ifp;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		NGE_CLRBIT(sc, NGE_RXFILT_CTL,
		    NGE_RXFILTCTL_MCHASH|NGE_RXFILTCTL_UCHASH);
		NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ALLMULTI);
		return;
	}

	/*
	 * We have to explicitly enable the multicast hash table
	 * on the NatSemi chip if we want to use it, which we do.
	 * We also have to tell it that we don't want to use the
	 * hash table for matching unicast addresses.
	 */
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_MCHASH);
	NGE_CLRBIT(sc, NGE_RXFILT_CTL,
	    NGE_RXFILTCTL_ALLMULTI|NGE_RXFILTCTL_UCHASH);

	filtsave = CSR_READ_4(sc, NGE_RXFILT_CTL);

	/* first, zot all the existing hash bits */
	for (i = 0; i < NGE_MCAST_FILTER_LEN; i += 2) {
		CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_MCAST_LO + i);
		CSR_WRITE_4(sc, NGE_RXFILT_DATA, 0);
	}

	/*
	 * From the 11 bits returned by the crc routine, the top 7
	 * bits represent the 16-bit word in the mcast hash table
	 * that needs to be updated, and the lower 4 bits represent
	 * which bit within that byte needs to be set.
	 */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 21;
		index = (h >> 4) & 0x7F;
		bit = h & 0xF;
		CSR_WRITE_4(sc, NGE_RXFILT_CTL,
		    NGE_FILTADDR_MCAST_LO + (index * 2));
		NGE_SETBIT(sc, NGE_RXFILT_DATA, (1 << bit));
	}
	IF_ADDR_UNLOCK(ifp);

	CSR_WRITE_4(sc, NGE_RXFILT_CTL, filtsave);

	return;
}

static void
nge_reset(sc)
	struct nge_softc	*sc;
{
	register int		i;

	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RESET);

	for (i = 0; i < NGE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, NGE_CSR) & NGE_CSR_RESET))
			break;
	}

	if (i == NGE_TIMEOUT)
		device_printf(sc->nge_dev, "reset never completed\n");

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	/*
	 * If this is a NetSemi chip, make sure to clear
	 * PME mode.
	 */
	CSR_WRITE_4(sc, NGE_CLKRUN, NGE_CLKRUN_PMESTS);
	CSR_WRITE_4(sc, NGE_CLKRUN, 0);

        return;
}

/*
 * Probe for a NatSemi chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
nge_probe(dev)
	device_t		dev;
{
	struct nge_type		*t;

	t = nge_devs;

	while(t->nge_name != NULL) {
		if ((pci_get_vendor(dev) == t->nge_vid) &&
		    (pci_get_device(dev) == t->nge_did)) {
			device_set_desc(dev, t->nge_name);
			return(BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
nge_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct nge_softc	*sc;
	struct ifnet		*ifp = NULL;
	int			error = 0, rid;

	sc = device_get_softc(dev);
	sc->nge_dev = dev;

	NGE_LOCK_INIT(sc, device_get_nameunit(dev));
	callout_init_mtx(&sc->nge_stat_ch, &sc->nge_mtx, 0);

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = NGE_RID;
	sc->nge_res = bus_alloc_resource_any(dev, NGE_RES, &rid, RF_ACTIVE);

	if (sc->nge_res == NULL) {
		device_printf(dev, "couldn't map ports/memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->nge_btag = rman_get_bustag(sc->nge_res);
	sc->nge_bhandle = rman_get_bushandle(sc->nge_res);

	/* Allocate interrupt */
	rid = 0;
	sc->nge_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->nge_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	nge_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	nge_read_eeprom(sc, (caddr_t)&eaddr[4], NGE_EE_NODEADDR, 1, 0);
	nge_read_eeprom(sc, (caddr_t)&eaddr[2], NGE_EE_NODEADDR + 1, 1, 0);
	nge_read_eeprom(sc, (caddr_t)&eaddr[0], NGE_EE_NODEADDR + 2, 1, 0);

	sc->nge_ldata = contigmalloc(sizeof(struct nge_list_data), M_DEVBUF,
	    M_NOWAIT|M_ZERO, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->nge_ldata == NULL) {
		device_printf(dev, "no memory for list buffers!\n");
		error = ENXIO;
		goto fail;
	}

	ifp = sc->nge_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nge_ioctl;
	ifp->if_start = nge_start;
	ifp->if_watchdog = nge_watchdog;
	ifp->if_init = nge_init;
	ifp->if_snd.ifq_maxlen = NGE_TX_LIST_CNT - 1;
	ifp->if_hwassist = NGE_CSUM_FEATURES;
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_VLAN_HWTAGGING;
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/*
	 * Do MII setup.
	 */
	/* XXX: leaked on error */
	if (mii_phy_probe(dev, &sc->nge_miibus,
			  nge_ifmedia_upd, nge_ifmedia_sts)) {
		if (CSR_READ_4(sc, NGE_CFG) & NGE_CFG_TBI_EN) {
			sc->nge_tbi = 1;
			device_printf(dev, "Using TBI\n");
			
			sc->nge_miibus = dev;

			ifmedia_init(&sc->nge_ifmedia, 0, nge_ifmedia_upd, 
				nge_ifmedia_sts);
#define	ADD(m, c)	ifmedia_add(&sc->nge_ifmedia, (m), (c), NULL)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, 0), 0);
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, 0, 0), 0);
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX, 0),0);
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0), 0);
#undef ADD
			device_printf(dev, " 1000baseSX, 1000baseSX-FDX, auto\n");
			
			ifmedia_set(&sc->nge_ifmedia, 
				IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0));
	    
			CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
				| NGE_GPIO_GP4_OUT 
				| NGE_GPIO_GP1_OUTENB | NGE_GPIO_GP2_OUTENB 
				| NGE_GPIO_GP3_OUTENB
				| NGE_GPIO_GP3_IN | NGE_GPIO_GP4_IN);
	    
		} else {
			device_printf(dev, "MII without any PHY!\n");
			error = ENXIO;
			goto fail;
		}
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/*
	 * Hookup IRQ last.
	 */
	error = bus_setup_intr(dev, sc->nge_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, nge_intr, sc, &sc->nge_intrhand);
	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		goto fail;
	}

	return (0);

fail:
	if (sc->nge_ldata)
		contigfree(sc->nge_ldata,
		  sizeof(struct nge_list_data), M_DEVBUF);
	if (ifp)
		if_free(ifp);
	if (sc->nge_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->nge_irq);
	if (sc->nge_res)
		bus_release_resource(dev, NGE_RES, NGE_RID, sc->nge_res);
	NGE_LOCK_DESTROY(sc);
	return(error);
}

static int
nge_detach(dev)
	device_t		dev;
{
	struct nge_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	ifp = sc->nge_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif
	NGE_LOCK(sc);
	nge_reset(sc);
	nge_stop(sc);
	NGE_UNLOCK(sc);
	callout_drain(&sc->nge_stat_ch);
	ether_ifdetach(ifp);

	bus_generic_detach(dev);
	if (!sc->nge_tbi) {
		device_delete_child(dev, sc->nge_miibus);
	}
	bus_teardown_intr(dev, sc->nge_irq, sc->nge_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->nge_irq);
	bus_release_resource(dev, NGE_RES, NGE_RID, sc->nge_res);

	contigfree(sc->nge_ldata, sizeof(struct nge_list_data), M_DEVBUF);
	if_free(ifp);

	NGE_LOCK_DESTROY(sc);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
nge_list_tx_init(sc)
	struct nge_softc	*sc;
{
	struct nge_list_data	*ld;
	struct nge_ring_data	*cd;
	int			i;

	cd = &sc->nge_cdata;
	ld = sc->nge_ldata;

	for (i = 0; i < NGE_TX_LIST_CNT; i++) {
		if (i == (NGE_TX_LIST_CNT - 1)) {
			ld->nge_tx_list[i].nge_nextdesc =
			    &ld->nge_tx_list[0];
			ld->nge_tx_list[i].nge_next =
			    vtophys(&ld->nge_tx_list[0]);
		} else {
			ld->nge_tx_list[i].nge_nextdesc =
			    &ld->nge_tx_list[i + 1];
			ld->nge_tx_list[i].nge_next =
			    vtophys(&ld->nge_tx_list[i + 1]);
		}
		ld->nge_tx_list[i].nge_mbuf = NULL;
		ld->nge_tx_list[i].nge_ptr = 0;
		ld->nge_tx_list[i].nge_ctl = 0;
	}

	cd->nge_tx_prod = cd->nge_tx_cons = cd->nge_tx_cnt = 0;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
nge_list_rx_init(sc)
	struct nge_softc	*sc;
{
	struct nge_list_data	*ld;
	struct nge_ring_data	*cd;
	int			i;

	ld = sc->nge_ldata;
	cd = &sc->nge_cdata;

	for (i = 0; i < NGE_RX_LIST_CNT; i++) {
		if (nge_newbuf(sc, &ld->nge_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (NGE_RX_LIST_CNT - 1)) {
			ld->nge_rx_list[i].nge_nextdesc =
			    &ld->nge_rx_list[0];
			ld->nge_rx_list[i].nge_next =
			    vtophys(&ld->nge_rx_list[0]);
		} else {
			ld->nge_rx_list[i].nge_nextdesc =
			    &ld->nge_rx_list[i + 1];
			ld->nge_rx_list[i].nge_next =
			    vtophys(&ld->nge_rx_list[i + 1]);
		}
	}

	cd->nge_rx_prod = 0;
	sc->nge_head = sc->nge_tail = NULL;

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
nge_newbuf(sc, c, m)
	struct nge_softc	*sc;
	struct nge_desc		*c;
	struct mbuf		*m;
{

	if (m == NULL) {
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			return (ENOBUFS);
	} else
		m->m_data = m->m_ext.ext_buf;

	m->m_len = m->m_pkthdr.len = MCLBYTES;

	m_adj(m, sizeof(u_int64_t));

	c->nge_mbuf = m;
	c->nge_ptr = vtophys(mtod(m, caddr_t));
	c->nge_ctl = m->m_len;
	c->nge_extsts = 0;

	return(0);
}

#ifdef NGE_FIXUP_RX
static __inline void
nge_fixup_rx(m)
	struct mbuf		*m;
{  
        int			i;
        uint16_t		*src, *dst;
   
	src = mtod(m, uint16_t *);
	dst = src - 1;
   
	for (i = 0; i < (m->m_len / sizeof(uint16_t) + 1); i++)
		*dst++ = *src++;
   
	m->m_data -= ETHER_ALIGN;
   
	return;
}  
#endif

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
nge_rxeof(sc)
	struct nge_softc	*sc;
{
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct nge_desc		*cur_rx;
	int			i, total_len = 0;
	u_int32_t		rxstat;

	NGE_LOCK_ASSERT(sc);
	ifp = sc->nge_ifp;
	i = sc->nge_cdata.nge_rx_prod;

	while(NGE_OWNDESC(&sc->nge_ldata->nge_rx_list[i])) {
		u_int32_t		extsts;

#ifdef DEVICE_POLLING
		if (ifp->if_capenable & IFCAP_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif

		cur_rx = &sc->nge_ldata->nge_rx_list[i];
		rxstat = cur_rx->nge_rxstat;
		extsts = cur_rx->nge_extsts;
		m = cur_rx->nge_mbuf;
		cur_rx->nge_mbuf = NULL;
		total_len = NGE_RXBYTES(cur_rx);
		NGE_INC(i, NGE_RX_LIST_CNT);

		if (rxstat & NGE_CMDSTS_MORE) {
			m->m_len = total_len;
			if (sc->nge_head == NULL) {
				m->m_pkthdr.len = total_len;
				sc->nge_head = sc->nge_tail = m;
			} else {
				m->m_flags &= ~M_PKTHDR;
				sc->nge_head->m_pkthdr.len += total_len;
				sc->nge_tail->m_next = m;
				sc->nge_tail = m;
			}
			nge_newbuf(sc, cur_rx, NULL);
			continue;
		}

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (!(rxstat & NGE_CMDSTS_PKT_OK)) {
			ifp->if_ierrors++;
			if (sc->nge_head != NULL) {
				m_freem(sc->nge_head);
				sc->nge_head = sc->nge_tail = NULL;
			}
			nge_newbuf(sc, cur_rx, m);
			continue;
		}

		/* Try conjure up a replacement mbuf. */

		if (nge_newbuf(sc, cur_rx, NULL)) {
			ifp->if_ierrors++;
			if (sc->nge_head != NULL) {
				m_freem(sc->nge_head);
				sc->nge_head = sc->nge_tail = NULL;
			}
			nge_newbuf(sc, cur_rx, m);
			continue;
		}

		if (sc->nge_head != NULL) {
			m->m_len = total_len;
			m->m_flags &= ~M_PKTHDR;
			sc->nge_tail->m_next = m;
			m = sc->nge_head;
			m->m_pkthdr.len += total_len;
			sc->nge_head = sc->nge_tail = NULL;
		} else
			m->m_pkthdr.len = m->m_len = total_len;

		/*
		 * Ok. NatSemi really screwed up here. This is the
		 * only gigE chip I know of with alignment constraints
		 * on receive buffers. RX buffers must be 64-bit aligned.
		 */
		/*
		 * By popular demand, ignore the alignment problems
		 * on the Intel x86 platform. The performance hit
		 * incurred due to unaligned accesses is much smaller
		 * than the hit produced by forcing buffer copies all
		 * the time, especially with jumbo frames. We still
		 * need to fix up the alignment everywhere else though.
		 */
#ifdef NGE_FIXUP_RX
		nge_fixup_rx(m);
#endif

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do IP checksum checking. */
		if (extsts & NGE_RXEXTSTS_IPPKT)
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
		if (!(extsts & NGE_RXEXTSTS_IPCSUMERR))
			m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		if ((extsts & NGE_RXEXTSTS_TCPPKT &&
		    !(extsts & NGE_RXEXTSTS_TCPCSUMERR)) ||
		    (extsts & NGE_RXEXTSTS_UDPPKT &&
		    !(extsts & NGE_RXEXTSTS_UDPCSUMERR))) {
			m->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}

		/*
		 * If we received a packet with a vlan tag, pass it
		 * to vlan_input() instead of ether_input().
		 */
		if (extsts & NGE_RXEXTSTS_VLANPKT) {
			m->m_pkthdr.ether_vtag =
			    ntohs(extsts & NGE_RXEXTSTS_VTCI);
			m->m_flags |= M_VLANTAG;
		}
		NGE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		NGE_LOCK(sc);
	}

	sc->nge_cdata.nge_rx_prod = i;

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
nge_txeof(sc)
	struct nge_softc	*sc;
{
	struct nge_desc		*cur_tx;
	struct ifnet		*ifp;
	u_int32_t		idx;

	NGE_LOCK_ASSERT(sc);
	ifp = sc->nge_ifp;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->nge_cdata.nge_tx_cons;
	while (idx != sc->nge_cdata.nge_tx_prod) {
		cur_tx = &sc->nge_ldata->nge_tx_list[idx];

		if (NGE_OWNDESC(cur_tx))
			break;

		if (cur_tx->nge_ctl & NGE_CMDSTS_MORE) {
			sc->nge_cdata.nge_tx_cnt--;
			NGE_INC(idx, NGE_TX_LIST_CNT);
			continue;
		}

		if (!(cur_tx->nge_ctl & NGE_CMDSTS_PKT_OK)) {
			ifp->if_oerrors++;
			if (cur_tx->nge_txstat & NGE_TXSTAT_EXCESSCOLLS)
				ifp->if_collisions++;
			if (cur_tx->nge_txstat & NGE_TXSTAT_OUTOFWINCOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions +=
		    (cur_tx->nge_txstat & NGE_TXSTAT_COLLCNT) >> 16;

		ifp->if_opackets++;
		if (cur_tx->nge_mbuf != NULL) {
			m_freem(cur_tx->nge_mbuf);
			cur_tx->nge_mbuf = NULL;
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		}

		sc->nge_cdata.nge_tx_cnt--;
		NGE_INC(idx, NGE_TX_LIST_CNT);
	}

	sc->nge_cdata.nge_tx_cons = idx;

	if (idx == sc->nge_cdata.nge_tx_prod)
		ifp->if_timer = 0;

	return;
}

static void
nge_tick(xsc)
	void			*xsc;
{
	struct nge_softc	*sc;
	struct mii_data		*mii;
	struct ifnet		*ifp;

	sc = xsc;
	NGE_LOCK_ASSERT(sc);
	ifp = sc->nge_ifp;

	if (sc->nge_tbi) {
		if (!sc->nge_link) {
			if (CSR_READ_4(sc, NGE_TBI_BMSR) 
			    & NGE_TBIBMSR_ANEG_DONE) {
				if (bootverbose)
					device_printf(sc->nge_dev, 
					    "gigabit link up\n");
				nge_miibus_statchg(sc->nge_miibus);
				sc->nge_link++;
				if (ifp->if_snd.ifq_head != NULL)
					nge_start_locked(ifp);
			}
		}
	} else {
		mii = device_get_softc(sc->nge_miibus);
		mii_tick(mii);

		if (!sc->nge_link) {
			if (mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
				sc->nge_link++;
				if (IFM_SUBTYPE(mii->mii_media_active) 
				    == IFM_1000_T && bootverbose)
					device_printf(sc->nge_dev, 
					    "gigabit link up\n");
				if (ifp->if_snd.ifq_head != NULL)
					nge_start_locked(ifp);
			}
		}
	}
	callout_reset(&sc->nge_stat_ch, hz, nge_tick, sc);

	return;
}

#ifdef DEVICE_POLLING
static poll_handler_t nge_poll;

static void
nge_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct  nge_softc *sc = ifp->if_softc;

	NGE_LOCK(sc);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		NGE_UNLOCK(sc);
		return;
	}

	/*
	 * On the nge, reading the status register also clears it.
	 * So before returning to intr mode we must make sure that all
	 * possible pending sources of interrupts have been served.
	 * In practice this means run to completion the *eof routines,
	 * and then call the interrupt routine
	 */
	sc->rxcycles = count;
	nge_rxeof(sc);
	nge_txeof(sc);
	if (ifp->if_snd.ifq_head != NULL)
		nge_start_locked(ifp);

	if (sc->rxcycles > 0 || cmd == POLL_AND_CHECK_STATUS) {
		u_int32_t	status;

		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, NGE_ISR);

		if (status & (NGE_ISR_RX_ERR|NGE_ISR_RX_OFLOW))
			nge_rxeof(sc);

		if (status & (NGE_ISR_RX_IDLE))
			NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RX_ENABLE);

		if (status & NGE_ISR_SYSERR) {
			nge_reset(sc);
			nge_init_locked(sc);
		}
	}
	NGE_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
nge_intr(arg)
	void			*arg;
{
	struct nge_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = sc->nge_ifp;

	NGE_LOCK(sc);
#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		NGE_UNLOCK(sc);
		return;
	}
#endif

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		nge_stop(sc);
		NGE_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, NGE_IER, 0);

	/* Data LED on for TBI mode */
	if(sc->nge_tbi)
		 CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
			     | NGE_GPIO_GP3_OUT);

	for (;;) {
		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, NGE_ISR);

		if ((status & NGE_INTRS) == 0)
			break;

		if ((status & NGE_ISR_TX_DESC_OK) ||
		    (status & NGE_ISR_TX_ERR) ||
		    (status & NGE_ISR_TX_OK) ||
		    (status & NGE_ISR_TX_IDLE))
			nge_txeof(sc);

		if ((status & NGE_ISR_RX_DESC_OK) ||
		    (status & NGE_ISR_RX_ERR) ||
		    (status & NGE_ISR_RX_OFLOW) ||
		    (status & NGE_ISR_RX_FIFO_OFLOW) ||
		    (status & NGE_ISR_RX_IDLE) ||
		    (status & NGE_ISR_RX_OK))
			nge_rxeof(sc);

		if ((status & NGE_ISR_RX_IDLE))
			NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RX_ENABLE);

		if (status & NGE_ISR_SYSERR) {
			nge_reset(sc);
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			nge_init_locked(sc);
		}

#if 0
		/* 
		 * XXX: nge_tick() is not ready to be called this way
		 * it screws up the aneg timeout because mii_tick() is
		 * only to be called once per second.
		 */
		if (status & NGE_IMR_PHY_INTR) {
			sc->nge_link = 0;
			nge_tick(sc);
		}
#endif
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, NGE_IER, 1);

	if (ifp->if_snd.ifq_head != NULL)
		nge_start_locked(ifp);

	/* Data LED off for TBI mode */

	if(sc->nge_tbi)
		CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
			    & ~NGE_GPIO_GP3_OUT);

	NGE_UNLOCK(sc);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
nge_encap(sc, m_head, txidx)
	struct nge_softc	*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct nge_desc		*f = NULL;
	struct mbuf		*m;
	int			frag, cur, cnt = 0;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	cur = frag = *txidx;

	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((NGE_TX_LIST_CNT -
			    (sc->nge_cdata.nge_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->nge_ldata->nge_tx_list[frag];
			f->nge_ctl = NGE_CMDSTS_MORE | m->m_len;
			f->nge_ptr = vtophys(mtod(m, vm_offset_t));
			if (cnt != 0)
				f->nge_ctl |= NGE_CMDSTS_OWN;
			cur = frag;
			NGE_INC(frag, NGE_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc->nge_ldata->nge_tx_list[*txidx].nge_extsts = 0;
	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			sc->nge_ldata->nge_tx_list[*txidx].nge_extsts |=
			    NGE_TXEXTSTS_IPCSUM;
		if (m_head->m_pkthdr.csum_flags & CSUM_TCP)
			sc->nge_ldata->nge_tx_list[*txidx].nge_extsts |=
			    NGE_TXEXTSTS_TCPCSUM;
		if (m_head->m_pkthdr.csum_flags & CSUM_UDP)
			sc->nge_ldata->nge_tx_list[*txidx].nge_extsts |=
			    NGE_TXEXTSTS_UDPCSUM;
	}

	if (m_head->m_flags & M_VLANTAG) {
		sc->nge_ldata->nge_tx_list[cur].nge_extsts |=
		    (NGE_TXEXTSTS_VLANPKT|htons(m_head->m_pkthdr.ether_vtag));
	}

	sc->nge_ldata->nge_tx_list[cur].nge_mbuf = m_head;
	sc->nge_ldata->nge_tx_list[cur].nge_ctl &= ~NGE_CMDSTS_MORE;
	sc->nge_ldata->nge_tx_list[*txidx].nge_ctl |= NGE_CMDSTS_OWN;
	sc->nge_cdata.nge_tx_cnt += cnt;
	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void
nge_start(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;

	sc = ifp->if_softc;
	NGE_LOCK(sc);
	nge_start_locked(ifp);
	NGE_UNLOCK(sc);
}

static void
nge_start_locked(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		idx;

	sc = ifp->if_softc;

	if (!sc->nge_link)
		return;

	idx = sc->nge_cdata.nge_tx_prod;

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	while(sc->nge_ldata->nge_tx_list[idx].nge_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (nge_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		ETHER_BPF_MTAP(ifp, m_head);

	}

	/* Transmit */
	sc->nge_cdata.nge_tx_prod = idx;
	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_TX_ENABLE);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void
nge_init(xsc)
	void			*xsc;
{
	struct nge_softc	*sc = xsc;

	NGE_LOCK(sc);
	nge_init_locked(sc);
	NGE_UNLOCK(sc);
}

static void
nge_init_locked(sc)
	struct nge_softc	*sc;
{
	struct ifnet		*ifp = sc->nge_ifp;
	struct mii_data		*mii;

	NGE_LOCK_ASSERT(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	nge_stop(sc);

	if (sc->nge_tbi) {
		mii = NULL;
	} else {
		mii = device_get_softc(sc->nge_miibus);
	}

	/* Set MAC address */
	CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_PAR0);
	CSR_WRITE_4(sc, NGE_RXFILT_DATA,
	    ((u_int16_t *)IF_LLADDR(sc->nge_ifp))[0]);
	CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_PAR1);
	CSR_WRITE_4(sc, NGE_RXFILT_DATA,
	    ((u_int16_t *)IF_LLADDR(sc->nge_ifp))[1]);
	CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_PAR2);
	CSR_WRITE_4(sc, NGE_RXFILT_DATA,
	    ((u_int16_t *)IF_LLADDR(sc->nge_ifp))[2]);

	/* Init circular RX list. */
	if (nge_list_rx_init(sc) == ENOBUFS) {
		device_printf(sc->nge_dev, "initialization failed: no "
			"memory for rx buffers\n");
		nge_stop(sc);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	nge_list_tx_init(sc);

	/*
	 * For the NatSemi chip, we have to explicitly enable the
	 * reception of ARP frames, as well as turn on the 'perfect
	 * match' filter where we store the station address, otherwise
	 * we won't receive unicasts meant for this host.
	 */
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ARP);
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_PERFECT);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ALLPHYS);
	} else {
		NGE_CLRBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ALLPHYS);
	}

	/*
	 * Set the capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_BROAD);
	} else {
		NGE_CLRBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_BROAD);
	}

	/*
	 * Load the multicast filter.
	 */
	nge_setmulti(sc);

	/* Turn the receive filter on */
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ENABLE);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, NGE_RX_LISTPTR,
	    vtophys(&sc->nge_ldata->nge_rx_list[0]));
	CSR_WRITE_4(sc, NGE_TX_LISTPTR,
	    vtophys(&sc->nge_ldata->nge_tx_list[0]));

	/* Set RX configuration */
	CSR_WRITE_4(sc, NGE_RX_CFG, NGE_RXCFG);
	/*
	 * Enable hardware checksum validation for all IPv4
	 * packets, do not reject packets with bad checksums.
	 */
	CSR_WRITE_4(sc, NGE_VLAN_IP_RXCTL, NGE_VIPRXCTL_IPCSUM_ENB);

	/*
	 * Tell the chip to detect and strip VLAN tag info from
	 * received frames. The tag will be provided in the extsts
	 * field in the RX descriptors.
	 */
	NGE_SETBIT(sc, NGE_VLAN_IP_RXCTL,
	    NGE_VIPRXCTL_TAG_DETECT_ENB|NGE_VIPRXCTL_TAG_STRIP_ENB);

	/* Set TX configuration */
	CSR_WRITE_4(sc, NGE_TX_CFG, NGE_TXCFG);

	/*
	 * Enable TX IPv4 checksumming on a per-packet basis.
	 */
	CSR_WRITE_4(sc, NGE_VLAN_IP_TXCTL, NGE_VIPTXCTL_CSUM_PER_PKT);

	/*
	 * Tell the chip to insert VLAN tags on a per-packet basis as
	 * dictated by the code in the frame encapsulation routine.
	 */
	NGE_SETBIT(sc, NGE_VLAN_IP_TXCTL, NGE_VIPTXCTL_TAG_PER_PKT);

	/* Set full/half duplex mode. */
	if (sc->nge_tbi) {
		if ((sc->nge_ifmedia.ifm_cur->ifm_media & IFM_GMASK) 
		    == IFM_FDX) {
			NGE_SETBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_SETBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		} else {
			NGE_CLRBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_CLRBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		}
	} else {
		if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
			NGE_SETBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_SETBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		} else {
			NGE_CLRBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_CLRBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);
		}
	}

	nge_tick(sc);

	/*
	 * Enable the delivery of PHY interrupts based on
	 * link/speed/duplex status changes. Also enable the
	 * extsts field in the DMA descriptors (needed for
	 * TCP/IP checksum offload on transmit).
	 */
	NGE_SETBIT(sc, NGE_CFG, NGE_CFG_PHYINTR_SPD|
	    NGE_CFG_PHYINTR_LNK|NGE_CFG_PHYINTR_DUP|NGE_CFG_EXTSTS_ENB);

	/*
	 * Configure interrupt holdoff (moderation). We can
	 * have the chip delay interrupt delivery for a certain
	 * period. Units are in 100us, and the max setting
	 * is 25500us (0xFF x 100us). Default is a 100us holdoff.
	 */
	CSR_WRITE_4(sc, NGE_IHR, 0x01);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, NGE_IMR, NGE_INTRS);
#ifdef DEVICE_POLLING
	/*
	 * ... only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		CSR_WRITE_4(sc, NGE_IER, 0);
	else
#endif
	CSR_WRITE_4(sc, NGE_IER, 1);

	/* Enable receiver and transmitter. */
	NGE_CLRBIT(sc, NGE_CSR, NGE_CSR_TX_DISABLE|NGE_CSR_RX_DISABLE);
	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RX_ENABLE);

	nge_ifmedia_upd_locked(ifp);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

/*
 * Set media options.
 */
static int
nge_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;

	sc = ifp->if_softc;
	NGE_LOCK(sc);
	nge_ifmedia_upd_locked(ifp);
	NGE_UNLOCK(sc);
	return (0);
}

static void
nge_ifmedia_upd_locked(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	NGE_LOCK_ASSERT(sc);

	if (sc->nge_tbi) {
		if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media) 
		     == IFM_AUTO) {
			CSR_WRITE_4(sc, NGE_TBI_ANAR, 
				CSR_READ_4(sc, NGE_TBI_ANAR)
					| NGE_TBIANAR_HDX | NGE_TBIANAR_FDX
					| NGE_TBIANAR_PS1 | NGE_TBIANAR_PS2);
			CSR_WRITE_4(sc, NGE_TBI_BMCR, NGE_TBIBMCR_ENABLE_ANEG
				| NGE_TBIBMCR_RESTART_ANEG);
			CSR_WRITE_4(sc, NGE_TBI_BMCR, NGE_TBIBMCR_ENABLE_ANEG);
		} else if ((sc->nge_ifmedia.ifm_cur->ifm_media 
			    & IFM_GMASK) == IFM_FDX) {
			NGE_SETBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_SETBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);

			CSR_WRITE_4(sc, NGE_TBI_ANAR, 0);
			CSR_WRITE_4(sc, NGE_TBI_BMCR, 0);
		} else {
			NGE_CLRBIT(sc, NGE_TX_CFG,
			    (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR));
			NGE_CLRBIT(sc, NGE_RX_CFG, NGE_RXCFG_RX_FDX);

			CSR_WRITE_4(sc, NGE_TBI_ANAR, 0);
			CSR_WRITE_4(sc, NGE_TBI_BMCR, 0);
		}
			
		CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
			    & ~NGE_GPIO_GP3_OUT);
	} else {
		mii = device_get_softc(sc->nge_miibus);
		sc->nge_link = 0;
		if (mii->mii_instance) {
			struct mii_softc	*miisc;

			LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
				mii_phy_reset(miisc);
		}
		mii_mediachg(mii);
	}
}

/*
 * Report current media status.
 */
static void
nge_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct nge_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	NGE_LOCK(sc);
	if (sc->nge_tbi) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;

		if (CSR_READ_4(sc, NGE_TBI_BMSR) & NGE_TBIBMSR_ANEG_DONE) {
			ifmr->ifm_status |= IFM_ACTIVE;
		} 
		if (CSR_READ_4(sc, NGE_TBI_BMCR) & NGE_TBIBMCR_LOOPBACK)
			ifmr->ifm_active |= IFM_LOOP;
		if (!CSR_READ_4(sc, NGE_TBI_BMSR) & NGE_TBIBMSR_ANEG_DONE) {
			ifmr->ifm_active |= IFM_NONE;
			ifmr->ifm_status = 0;
			NGE_UNLOCK(sc);
			return;
		} 
		ifmr->ifm_active |= IFM_1000_SX;
		if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media)
		    == IFM_AUTO) {
			ifmr->ifm_active |= IFM_AUTO;
			if (CSR_READ_4(sc, NGE_TBI_ANLPAR)
			    & NGE_TBIANAR_FDX) {
				ifmr->ifm_active |= IFM_FDX;
			}else if (CSR_READ_4(sc, NGE_TBI_ANLPAR)
				  & NGE_TBIANAR_HDX) {
				ifmr->ifm_active |= IFM_HDX;
			}
		} else if ((sc->nge_ifmedia.ifm_cur->ifm_media & IFM_GMASK) 
			== IFM_FDX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
 
	} else {
		mii = device_get_softc(sc->nge_miibus);
		mii_pollstat(mii);
		ifmr->ifm_active = mii->mii_media_active;
		ifmr->ifm_status = mii->mii_media_status;
	}
	NGE_UNLOCK(sc);

	return;
}

static int
nge_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct nge_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	switch(command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > NGE_JUMBO_MTU)
			error = EINVAL;
		else {
			NGE_LOCK(sc);
			ifp->if_mtu = ifr->ifr_mtu;
			/*
			 * Workaround: if the MTU is larger than
			 * 8152 (TX FIFO size minus 64 minus 18), turn off
			 * TX checksum offloading.
			 */
			if (ifr->ifr_mtu >= 8152) {
				ifp->if_capenable &= ~IFCAP_TXCSUM;
				ifp->if_hwassist = 0;
			} else {
				ifp->if_capenable |= IFCAP_TXCSUM;
				ifp->if_hwassist = NGE_CSUM_FEATURES;
			}
			NGE_UNLOCK(sc);
		}
		break;
	case SIOCSIFFLAGS:
		NGE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->nge_if_flags & IFF_PROMISC)) {
				NGE_SETBIT(sc, NGE_RXFILT_CTL,
				    NGE_RXFILTCTL_ALLPHYS|
				    NGE_RXFILTCTL_ALLMULTI);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->nge_if_flags & IFF_PROMISC) {
				NGE_CLRBIT(sc, NGE_RXFILT_CTL,
				    NGE_RXFILTCTL_ALLPHYS);
				if (!(ifp->if_flags & IFF_ALLMULTI))
					NGE_CLRBIT(sc, NGE_RXFILT_CTL,
					    NGE_RXFILTCTL_ALLMULTI);
			} else {
				ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
				nge_init_locked(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				nge_stop(sc);
		}
		sc->nge_if_flags = ifp->if_flags;
		NGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		NGE_LOCK(sc);
		nge_setmulti(sc);
		NGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->nge_tbi) {
			error = ifmedia_ioctl(ifp, ifr, &sc->nge_ifmedia, 
					      command);
		} else {
			mii = device_get_softc(sc->nge_miibus);
			error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, 
					      command);
		}
		break;
	case SIOCSIFCAP:
#ifdef DEVICE_POLLING
		if (ifr->ifr_reqcap & IFCAP_POLLING &&
		    !(ifp->if_capenable & IFCAP_POLLING)) {
			error = ether_poll_register(nge_poll, ifp);
			if (error)
				return(error);
			NGE_LOCK(sc);
			/* Disable interrupts */
			CSR_WRITE_4(sc, NGE_IER, 0);
			ifp->if_capenable |= IFCAP_POLLING;
			NGE_UNLOCK(sc);
			return (error);
			
		}
		if (!(ifr->ifr_reqcap & IFCAP_POLLING) &&
		    ifp->if_capenable & IFCAP_POLLING) {
			error = ether_poll_deregister(ifp);
			/* Enable interrupts. */
			NGE_LOCK(sc);
			CSR_WRITE_4(sc, NGE_IER, 1);
			ifp->if_capenable &= ~IFCAP_POLLING;
			NGE_UNLOCK(sc);
			return (error);
		}
#endif /* DEVICE_POLLING */
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return(error);
}

static void
nge_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	if_printf(ifp, "watchdog timeout\n");

	NGE_LOCK(sc);
	nge_stop(sc);
	nge_reset(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	nge_init_locked(sc);

	if (ifp->if_snd.ifq_head != NULL)
		nge_start_locked(ifp);

	NGE_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
nge_stop(sc)
	struct nge_softc	*sc;
{
	register int		i;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	NGE_LOCK_ASSERT(sc);
	ifp = sc->nge_ifp;
	ifp->if_timer = 0;
	if (sc->nge_tbi) {
		mii = NULL;
	} else {
		mii = device_get_softc(sc->nge_miibus);
	}

	callout_stop(&sc->nge_stat_ch);
	CSR_WRITE_4(sc, NGE_IER, 0);
	CSR_WRITE_4(sc, NGE_IMR, 0);
	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_TX_DISABLE|NGE_CSR_RX_DISABLE);
	DELAY(1000);
	CSR_WRITE_4(sc, NGE_TX_LISTPTR, 0);
	CSR_WRITE_4(sc, NGE_RX_LISTPTR, 0);

	if (!sc->nge_tbi)
		mii_down(mii);

	sc->nge_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < NGE_RX_LIST_CNT; i++) {
		if (sc->nge_ldata->nge_rx_list[i].nge_mbuf != NULL) {
			m_freem(sc->nge_ldata->nge_rx_list[i].nge_mbuf);
			sc->nge_ldata->nge_rx_list[i].nge_mbuf = NULL;
		}
	}
	bzero((char *)&sc->nge_ldata->nge_rx_list,
		sizeof(sc->nge_ldata->nge_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < NGE_TX_LIST_CNT; i++) {
		if (sc->nge_ldata->nge_tx_list[i].nge_mbuf != NULL) {
			m_freem(sc->nge_ldata->nge_tx_list[i].nge_mbuf);
			sc->nge_ldata->nge_tx_list[i].nge_mbuf = NULL;
		}
	}

	bzero((char *)&sc->nge_ldata->nge_tx_list,
		sizeof(sc->nge_ldata->nge_tx_list));

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
nge_shutdown(dev)
	device_t		dev;
{
	struct nge_softc	*sc;

	sc = device_get_softc(dev);

	NGE_LOCK(sc);
	nge_reset(sc);
	nge_stop(sc);
	NGE_UNLOCK(sc);

	return;
}
