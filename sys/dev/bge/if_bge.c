/*-
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2001
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/dev/bge/if_bge.c,v 1.91.2.13 2006/03/04 09:34:48 oleg Exp $");

/*
 * Broadcom BCM570x family gigabit ethernet driver for FreeBSD.
 *
 * The Broadcom BCM5700 is based on technology originally developed by
 * Alteon Networks as part of the Tigon I and Tigon II gigabit ethernet
 * MAC chips. The BCM5700, sometimes refered to as the Tigon III, has
 * two on-board MIPS R4000 CPUs and can have as much as 16MB of external
 * SSRAM. The BCM5700 supports TCP, UDP and IP checksum offload, jumbo
 * frames, highly configurable RX filtering, and 16 RX and TX queues
 * (which, along with RX filter rules, can be used for QOS applications).
 * Other features, such as TCP segmentation, may be available as part
 * of value-added firmware updates. Unlike the Tigon I and Tigon II,
 * firmware images can be stored in hardware and need not be compiled
 * into the driver.
 *
 * The BCM5700 supports the PCI v2.2 and PCI-X v1.0 standards, and will
 * function in a 32-bit/64-bit 33/66Mhz bus, or a 64-bit/133Mhz bus.
 *
 * The BCM5701 is a single-chip solution incorporating both the BCM5700
 * MAC and a BCM5401 10/100/1000 PHY. Unlike the BCM5700, the BCM5701
 * does not support external SSRAM.
 *
 * Broadcom also produces a variation of the BCM5700 under the "Altima"
 * brand name, which is functionally similar but lacks PCI-X support.
 *
 * Without external SSRAM, you can only have at most 4 TX rings,
 * and the use of the mini RX ring is disabled. This seems to imply
 * that these features are simply not available on the BCM5701. As a
 * result, this driver does not implement any support for the mini RX
 * ring.
 */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <machine/clock.h>      /* for DELAY */
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"
#include <dev/mii/brgphyreg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/bge/if_bgereg.h>

#include "opt_bge.h"

#define BGE_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)
#define ETHER_MIN_NOPAD		(ETHER_MIN_LEN - ETHER_CRC_LEN) /* i.e., 60 */

MODULE_DEPEND(bge, pci, 1, 1, 1);
MODULE_DEPEND(bge, ether, 1, 1, 1);
MODULE_DEPEND(bge, miibus, 1, 1, 1);

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Various supported device vendors/types and their names. Note: the
 * spec seems to indicate that the hardware still has Alteon's vendor
 * ID burned into it, though it will always be overriden by the vendor
 * ID in the EEPROM. Just to be safe, we cover all possibilities.
 */
#define BGE_DEVDESC_MAX		64	/* Maximum device description length */

static struct bge_type bge_devs[] = {
	{ ALT_VENDORID,	ALT_DEVICEID_BCM5700,
		"Broadcom BCM5700 Gigabit Ethernet" },
	{ ALT_VENDORID,	ALT_DEVICEID_BCM5701,
		"Broadcom BCM5701 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5700,
		"Broadcom BCM5700 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5701,
		"Broadcom BCM5701 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5702,
		"Broadcom BCM5702 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5702X,
		"Broadcom BCM5702X Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5703,
		"Broadcom BCM5703 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5703X,
		"Broadcom BCM5703X Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5704C,
		"Broadcom BCM5704C Dual Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5704S,
		"Broadcom BCM5704S Dual Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705,
		"Broadcom BCM5705 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705K,
		"Broadcom BCM5705K Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705M,
		"Broadcom BCM5705M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705M_ALT,
		"Broadcom BCM5705M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5714C,
		"Broadcom BCM5714C Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5721,
		"Broadcom BCM5721 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5750,
		"Broadcom BCM5750 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5750M,
		"Broadcom BCM5750M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5751,
		"Broadcom BCM5751 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5751M,
		"Broadcom BCM5751M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5752,
		"Broadcom BCM5752 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5782,
		"Broadcom BCM5782 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5788,
		"Broadcom BCM5788 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5789,
		"Broadcom BCM5789 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5901,
		"Broadcom BCM5901 Fast Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5901A2,
		"Broadcom BCM5901A2 Fast Ethernet" },
	{ SK_VENDORID, SK_DEVICEID_ALTIMA,
		"SysKonnect Gigabit Ethernet" },
	{ ALTIMA_VENDORID, ALTIMA_DEVICE_AC1000,
		"Altima AC1000 Gigabit Ethernet" },
	{ ALTIMA_VENDORID, ALTIMA_DEVICE_AC1002,
		"Altima AC1002 Gigabit Ethernet" },
	{ ALTIMA_VENDORID, ALTIMA_DEVICE_AC9100,
		"Altima AC9100 Gigabit Ethernet" },
	{ 0, 0, NULL }
};

static int bge_probe		(device_t);
static int bge_attach		(device_t);
static int bge_detach		(device_t);
static int bge_suspend		(device_t);
static int bge_resume		(device_t);
static void bge_release_resources
				(struct bge_softc *);
static void bge_dma_map_addr	(void *, bus_dma_segment_t *, int, int);
static int bge_dma_alloc	(device_t);
static void bge_dma_free	(struct bge_softc *);

static void bge_txeof		(struct bge_softc *);
static void bge_rxeof		(struct bge_softc *);

static void bge_tick_locked	(struct bge_softc *);
static void bge_tick		(void *);
static void bge_stats_update	(struct bge_softc *);
static void bge_stats_update_regs
				(struct bge_softc *);
static int bge_encap		(struct bge_softc *, struct mbuf *,
					u_int32_t *);

static void bge_intr		(void *);
static void bge_start_locked	(struct ifnet *);
static void bge_start		(struct ifnet *);
static int bge_ioctl		(struct ifnet *, u_long, caddr_t);
static void bge_init_locked	(struct bge_softc *);
static void bge_init		(void *);
static void bge_stop		(struct bge_softc *);
static void bge_watchdog		(struct ifnet *);
static void bge_shutdown		(device_t);
static int bge_ifmedia_upd	(struct ifnet *);
static void bge_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static u_int8_t	bge_eeprom_getbyte	(struct bge_softc *, int, u_int8_t *);
static int bge_read_eeprom	(struct bge_softc *, caddr_t, int, int);

static void bge_setmulti	(struct bge_softc *);

static int bge_newbuf_std	(struct bge_softc *, int, struct mbuf *);
static int bge_newbuf_jumbo	(struct bge_softc *, int, struct mbuf *);
static int bge_init_rx_ring_std	(struct bge_softc *);
static void bge_free_rx_ring_std	(struct bge_softc *);
static int bge_init_rx_ring_jumbo	(struct bge_softc *);
static void bge_free_rx_ring_jumbo	(struct bge_softc *);
static void bge_free_tx_ring	(struct bge_softc *);
static int bge_init_tx_ring	(struct bge_softc *);

static int bge_chipinit		(struct bge_softc *);
static int bge_blockinit	(struct bge_softc *);

#ifdef notdef
static u_int8_t bge_vpd_readbyte(struct bge_softc *, int);
static void bge_vpd_read_res	(struct bge_softc *, struct vpd_res *, int);
static void bge_vpd_read	(struct bge_softc *);
#endif

static u_int32_t bge_readmem_ind
				(struct bge_softc *, int);
static void bge_writemem_ind	(struct bge_softc *, int, int);
#ifdef notdef
static u_int32_t bge_readreg_ind
				(struct bge_softc *, int);
#endif
static void bge_writereg_ind	(struct bge_softc *, int, int);

static int bge_miibus_readreg	(device_t, int, int);
static int bge_miibus_writereg	(device_t, int, int, int);
static void bge_miibus_statchg	(device_t);
#ifdef DEVICE_POLLING
static void bge_poll		(struct ifnet *ifp, enum poll_cmd cmd,
				    int count);
static void bge_poll_locked	(struct ifnet *ifp, enum poll_cmd cmd,
				    int count);
#endif

static void bge_reset		(struct bge_softc *);
static void bge_link_upd	(struct bge_softc *);

static device_method_t bge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bge_probe),
	DEVMETHOD(device_attach,	bge_attach),
	DEVMETHOD(device_detach,	bge_detach),
	DEVMETHOD(device_shutdown,	bge_shutdown),
	DEVMETHOD(device_suspend,	bge_suspend),
	DEVMETHOD(device_resume,	bge_resume),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	bge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	bge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	bge_miibus_statchg),

	{ 0, 0 }
};

static driver_t bge_driver = {
	"bge",
	bge_methods,
	sizeof(struct bge_softc)
};

static devclass_t bge_devclass;

DRIVER_MODULE(bge, pci, bge_driver, bge_devclass, 0, 0);
DRIVER_MODULE(miibus, bge, miibus_driver, miibus_devclass, 0, 0);

static u_int32_t
bge_readmem_ind(sc, off)
	struct bge_softc *sc;
	int off;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, off, 4);
	return(pci_read_config(dev, BGE_PCI_MEMWIN_DATA, 4));
}

static void
bge_writemem_ind(sc, off, val)
	struct bge_softc *sc;
	int off, val;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, off, 4);
	pci_write_config(dev, BGE_PCI_MEMWIN_DATA, val, 4);

	return;
}

#ifdef notdef
static u_int32_t
bge_readreg_ind(sc, off)
	struct bge_softc *sc;
	int off;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_REG_BASEADDR, off, 4);
	return(pci_read_config(dev, BGE_PCI_REG_DATA, 4));
}
#endif

static void
bge_writereg_ind(sc, off, val)
	struct bge_softc *sc;
	int off, val;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_REG_BASEADDR, off, 4);
	pci_write_config(dev, BGE_PCI_REG_DATA, val, 4);

	return;
}

/*
 * Map a single buffer address.
 */

static void
bge_dma_map_addr(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg;
	int error;
{
	struct bge_dmamap_arg *ctx;

	if (error)
		return;

	ctx = arg;

	if (nseg > ctx->bge_maxsegs) {
		ctx->bge_maxsegs = 0;
		return;
	}

	ctx->bge_busaddr = segs->ds_addr;

	return;
}

#ifdef notdef
static u_int8_t
bge_vpd_readbyte(sc, addr)
	struct bge_softc *sc;
	int addr;
{
	int i;
	device_t dev;
	u_int32_t val;

	dev = sc->bge_dev;
	pci_write_config(dev, BGE_PCI_VPD_ADDR, addr, 2);
	for (i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (pci_read_config(dev, BGE_PCI_VPD_ADDR, 2) & BGE_VPD_FLAG)
			break;
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev, "VPD read timed out\n");
		return(0);
	}

	val = pci_read_config(dev, BGE_PCI_VPD_DATA, 4);

	return((val >> ((addr % 4) * 8)) & 0xFF);
}

static void
bge_vpd_read_res(sc, res, addr)
	struct bge_softc *sc;
	struct vpd_res *res;
	int addr;
{
	int i;
	u_int8_t *ptr;

	ptr = (u_int8_t *)res;
	for (i = 0; i < sizeof(struct vpd_res); i++)
		ptr[i] = bge_vpd_readbyte(sc, i + addr);

	return;
}

static void
bge_vpd_read(sc)
	struct bge_softc *sc;
{
	int pos = 0, i;
	struct vpd_res res;

	if (sc->bge_vpd_prodname != NULL)
		free(sc->bge_vpd_prodname, M_DEVBUF);
	if (sc->bge_vpd_readonly != NULL)
		free(sc->bge_vpd_readonly, M_DEVBUF);
	sc->bge_vpd_prodname = NULL;
	sc->bge_vpd_readonly = NULL;

	bge_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_ID) {
		device_printf(sc->bge_dev,
		    "bad VPD resource id: expected %x got %x\n", VPD_RES_ID,
		    res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->bge_vpd_prodname = malloc(res.vr_len + 1, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < res.vr_len; i++)
		sc->bge_vpd_prodname[i] = bge_vpd_readbyte(sc, i + pos);
	sc->bge_vpd_prodname[i] = '\0';
	pos += i;

	bge_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_READ) {
		device_printf(sc->bge_dev,
		    "bad VPD resource id: expected %x got %x\n", VPD_RES_READ,
		    res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->bge_vpd_readonly = malloc(res.vr_len, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < res.vr_len + 1; i++)
		sc->bge_vpd_readonly[i] = bge_vpd_readbyte(sc, i + pos);

	return;
}
#endif

/*
 * Read a byte of data stored in the EEPROM at address 'addr.' The
 * BCM570x supports both the traditional bitbang interface and an
 * auto access interface for reading the EEPROM. We use the auto
 * access method.
 */
static u_int8_t
bge_eeprom_getbyte(sc, addr, dest)
	struct bge_softc *sc;
	int addr;
	u_int8_t *dest;
{
	int i;
	u_int32_t byte = 0;

	/*
	 * Enable use of auto EEPROM access so we can avoid
	 * having to use the bitbang method.
	 */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);

	/* Reset the EEPROM, load the clock period. */
	CSR_WRITE_4(sc, BGE_EE_ADDR,
	    BGE_EEADDR_RESET|BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	DELAY(20);

	/* Issue the read EEPROM command. */
	CSR_WRITE_4(sc, BGE_EE_ADDR, BGE_EE_READCMD | addr);

	/* Wait for completion */
	for(i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_EE_ADDR) & BGE_EEADDR_DONE)
			break;
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev, "EEPROM read timed out\n");
		return(1);
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_EE_DATA);

	*dest = (byte >> ((addr % 4) * 8)) & 0xFF;

	return(0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int
bge_read_eeprom(sc, dest, off, cnt)
	struct bge_softc *sc;
	caddr_t dest;
	int off;
	int cnt;
{
	int err = 0, i;
	u_int8_t byte = 0;

	for (i = 0; i < cnt; i++) {
		err = bge_eeprom_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return(err ? 1 : 0);
}

static int
bge_miibus_readreg(dev, phy, reg)
	device_t dev;
	int phy, reg;
{
	struct bge_softc *sc;
	u_int32_t val, autopoll;
	int i;

	sc = device_get_softc(dev);

	/*
	 * Broadcom's own driver always assumes the internal
	 * PHY is at GMII address 1. On some chips, the PHY responds
	 * to accesses at all addresses, which could cause us to
	 * bogusly attach the PHY 32 times at probe type. Always
	 * restricting the lookup to address 1 is simpler than
	 * trying to figure out which chips revisions should be
	 * special-cased.
	 */
	if (phy != 1)
		return(0);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_READ|BGE_MICOMM_BUSY|
	    BGE_MIPHY(phy)|BGE_MIREG(reg));

	for (i = 0; i < BGE_TIMEOUT; i++) {
		val = CSR_READ_4(sc, BGE_MI_COMM);
		if (!(val & BGE_MICOMM_BUSY))
			break;
	}

	if (i == BGE_TIMEOUT) {
		if_printf(sc->bge_ifp, "PHY read timed out\n");
		val = 0;
		goto done;
	}

	val = CSR_READ_4(sc, BGE_MI_COMM);

done:
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (val & BGE_MICOMM_READFAIL)
		return(0);

	return(val & 0xFFFF);
}

static int
bge_miibus_writereg(dev, phy, reg, val)
	device_t dev;
	int phy, reg, val;
{
	struct bge_softc *sc;
	u_int32_t autopoll;
	int i;

	sc = device_get_softc(dev);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_WRITE|BGE_MICOMM_BUSY|
	    BGE_MIPHY(phy)|BGE_MIREG(reg)|val);

	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, BGE_MI_COMM) & BGE_MICOMM_BUSY))
			break;
	}

	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (i == BGE_TIMEOUT) {
		if_printf(sc->bge_ifp, "PHY read timed out\n");
		return(0);
	}

	return(0);
}

static void
bge_miibus_statchg(dev)
	device_t dev;
{
	struct bge_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->bge_miibus);

	BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_PORTMODE);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) {
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_GMII);
	} else {
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_MII);
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
	} else {
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
	}

	return;
}

/*
 * Intialize a standard receive ring descriptor.
 */
static int
bge_newbuf_std(sc, i, m)
	struct bge_softc	*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct bge_rx_bd	*r;
	struct bge_dmamap_arg	ctx;
	int			error;

	if (m == NULL) {
		m_new = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m_new == NULL)
			return(ENOBUFS);
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	if (!sc->bge_rx_alignment_bug)
		m_adj(m_new, ETHER_ALIGN);
	sc->bge_cdata.bge_rx_std_chain[i] = m_new;
	r = &sc->bge_ldata.bge_rx_std_ring[i];
	ctx.bge_maxsegs = 1;
	ctx.sc = sc;
	error = bus_dmamap_load(sc->bge_cdata.bge_mtag,
	    sc->bge_cdata.bge_rx_std_dmamap[i], mtod(m_new, void *),
	    m_new->m_len, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);
	if (error || ctx.bge_maxsegs == 0) {
		if (m == NULL) {
			sc->bge_cdata.bge_rx_std_chain[i] = NULL;
			m_freem(m_new);
		}
		return(ENOMEM);
	}
	r->bge_addr.bge_addr_lo = BGE_ADDR_LO(ctx.bge_busaddr);
	r->bge_addr.bge_addr_hi = BGE_ADDR_HI(ctx.bge_busaddr);
	r->bge_flags = BGE_RXBDFLAG_END;
	r->bge_len = m_new->m_len;
	r->bge_idx = i;

	bus_dmamap_sync(sc->bge_cdata.bge_mtag,
	    sc->bge_cdata.bge_rx_std_dmamap[i],
	    BUS_DMASYNC_PREREAD);

	return(0);
}

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int
bge_newbuf_jumbo(sc, i, m)
	struct bge_softc *sc;
	int i;
	struct mbuf *m;
{
	bus_dma_segment_t segs[BGE_NSEG_JUMBO];
	struct bge_extrx_bd *r;
	struct mbuf *m_new = NULL;
	int nsegs;
	int error;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);

		m_cljget(m_new, M_DONTWAIT, MJUM9BYTES);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MJUM9BYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MJUM9BYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	if (!sc->bge_rx_alignment_bug)
		m_adj(m_new, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_mtag_jumbo,
	    sc->bge_cdata.bge_rx_jumbo_dmamap[i],
	    m_new, segs, &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		if (m == NULL)
			m_freem(m_new);
		return(error);
	}
	sc->bge_cdata.bge_rx_jumbo_chain[i] = m_new;

	/*
	 * Fill in the extended RX buffer descriptor.
	 */
	r = &sc->bge_ldata.bge_rx_jumbo_ring[i];
	r->bge_flags = BGE_RXBDFLAG_JUMBO_RING|BGE_RXBDFLAG_END;
	r->bge_idx = i;
	r->bge_len3 = r->bge_len2 = r->bge_len1 = 0;
	switch (nsegs) {
	case 4:
		r->bge_addr3.bge_addr_lo = BGE_ADDR_LO(segs[3].ds_addr);
		r->bge_addr3.bge_addr_hi = BGE_ADDR_HI(segs[3].ds_addr);
		r->bge_len3 = segs[3].ds_len;
	case 3:
		r->bge_addr2.bge_addr_lo = BGE_ADDR_LO(segs[2].ds_addr);
		r->bge_addr2.bge_addr_hi = BGE_ADDR_HI(segs[2].ds_addr);
		r->bge_len2 = segs[2].ds_len;
	case 2:
		r->bge_addr1.bge_addr_lo = BGE_ADDR_LO(segs[1].ds_addr);
		r->bge_addr1.bge_addr_hi = BGE_ADDR_HI(segs[1].ds_addr);
		r->bge_len1 = segs[1].ds_len;
	case 1:
		r->bge_addr0.bge_addr_lo = BGE_ADDR_LO(segs[0].ds_addr);
		r->bge_addr0.bge_addr_hi = BGE_ADDR_HI(segs[0].ds_addr);
		r->bge_len0 = segs[0].ds_len;
		break;
	default:
		panic("%s: %d segments\n", __func__, nsegs);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_mtag,
	    sc->bge_cdata.bge_rx_jumbo_dmamap[i],
	    BUS_DMASYNC_PREREAD);

	return (0);
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
static int
bge_init_rx_ring_std(sc)
	struct bge_softc *sc;
{
	int i;

	for (i = 0; i < BGE_SSLOTS; i++) {
		if (bge_newbuf_std(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->bge_std = i - 1;
	CSR_WRITE_4(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);

	return(0);
}

static void
bge_free_rx_ring_std(sc)
	struct bge_softc *sc;
{
	int i;

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_chain[i] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i]);
			m_freem(sc->bge_cdata.bge_rx_std_chain[i]);
			sc->bge_cdata.bge_rx_std_chain[i] = NULL;
		}
		bzero((char *)&sc->bge_ldata.bge_rx_std_ring[i],
		    sizeof(struct bge_rx_bd));
	}

	return;
}

static int
bge_init_rx_ring_jumbo(sc)
	struct bge_softc *sc;
{
	struct bge_rcb *rcb;
	int i;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (bge_newbuf_jumbo(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
	    sc->bge_cdata.bge_rx_jumbo_ring_map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->bge_jumbo = i - 1;

	rcb = &sc->bge_ldata.bge_info.bge_jumbo_rx_rcb;
	rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0,
				    BGE_RCB_FLAG_USE_EXT_RX_BD);
	CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);

	CSR_WRITE_4(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);

	return(0);
}

static void
bge_free_rx_ring_jumbo(sc)
	struct bge_softc *sc;
{
	int i;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_chain[i] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
			m_freem(sc->bge_cdata.bge_rx_jumbo_chain[i]);
			sc->bge_cdata.bge_rx_jumbo_chain[i] = NULL;
		}
		bzero((char *)&sc->bge_ldata.bge_rx_jumbo_ring[i],
		    sizeof(struct bge_extrx_bd));
	}

	return;
}

static void
bge_free_tx_ring(sc)
	struct bge_softc *sc;
{
	int i;

	if (sc->bge_ldata.bge_tx_ring == NULL)
		return;

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_chain[i] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i]);
			m_freem(sc->bge_cdata.bge_tx_chain[i]);
			sc->bge_cdata.bge_tx_chain[i] = NULL;
		}
		bzero((char *)&sc->bge_ldata.bge_tx_ring[i],
		    sizeof(struct bge_tx_bd));
	}

	return;
}

static int
bge_init_tx_ring(sc)
	struct bge_softc *sc;
{
	sc->bge_txcnt = 0;
	sc->bge_tx_saved_considx = 0;

	/* Initialize transmit producer index for host-memory send ring. */
	sc->bge_tx_prodidx = 0;
	CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);

	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);

	/* NIC-memory send ring not used; initialize to zero. */
	CSR_WRITE_4(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);
	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		CSR_WRITE_4(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);

	return(0);
}

static void
bge_setmulti(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	u_int32_t hashes[4] = { 0, 0, 0, 0 };
	int h, i;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		for (i = 0; i < 4; i++)
			CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), 0xFFFFFFFF);
		return;
	}

	/* First, zot all the existing filters. */
	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), 0);

	/* Now program new ones. */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) & 0x7F;
		hashes[(h & 0x60) >> 5] |= 1 << (h & 0x1F);
	}
	IF_ADDR_UNLOCK(ifp);

	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), hashes[i]);

	return;
}

/*
 * Do endian, PCI and DMA initialization. Also check the on-board ROM
 * self-test results.
 */
static int
bge_chipinit(sc)
	struct bge_softc *sc;
{
	int			i;
	u_int32_t		dma_rw_ctl;

	/* Set endian type before we access any non-PCI registers. */
	pci_write_config(sc->bge_dev, BGE_PCI_MISC_CTL, BGE_INIT, 4);

	/*
	 * Check the 'ROM failed' bit on the RX CPU to see if
	 * self-tests passed.
	 */
	if (CSR_READ_4(sc, BGE_RXCPU_MODE) & BGE_RXCPUMODE_ROMFAIL) {
		device_printf(sc->bge_dev, "RX CPU self-diagnostics failed!\n");
		return(ENODEV);
	}

	/* Clear the MAC control register */
	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * Clear the MAC statistics block in the NIC's
	 * internal memory.
	 */
	for (i = BGE_STATS_BLOCK;
	    i < BGE_STATS_BLOCK_END + 1; i += sizeof(u_int32_t))
		BGE_MEMWIN_WRITE(sc, i, 0);

	for (i = BGE_STATUS_BLOCK;
	    i < BGE_STATUS_BLOCK_END + 1; i += sizeof(u_int32_t))
		BGE_MEMWIN_WRITE(sc, i, 0);

	/* Set up the PCI DMA control register. */
	if (sc->bge_pcie) {
		dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
		    (0xf << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
		    (0x2 << BGE_PCIDMARWCTL_WR_WAT_SHIFT);
	} else if (pci_read_config(sc->bge_dev, BGE_PCI_PCISTATE, 4) &
	    BGE_PCISTATE_PCI_BUSMODE) {
		/* Conventional PCI bus */
		dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
		    (0x7 << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
		    (0x7 << BGE_PCIDMARWCTL_WR_WAT_SHIFT) |
		    (0x0F);
	} else {
		/* PCI-X bus */
		/*
		 * The 5704 uses a different encoding of read/write
		 * watermarks.
		 */
		if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
			dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
			    (0x7 << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
			    (0x3 << BGE_PCIDMARWCTL_WR_WAT_SHIFT);
		else
			dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
			    (0x3 << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
			    (0x3 << BGE_PCIDMARWCTL_WR_WAT_SHIFT) |
			    (0x0F);

		/*
		 * 5703 and 5704 need ONEDMA_AT_ONCE as a workaround
		 * for hardware bugs.
		 */
		if (sc->bge_asicrev == BGE_ASICREV_BCM5703 ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5704) {
			u_int32_t tmp;

			tmp = CSR_READ_4(sc, BGE_PCI_CLKCTL) & 0x1f;
			if (tmp == 0x6 || tmp == 0x7)
				dma_rw_ctl |= BGE_PCIDMARWCTL_ONEDMA_ATONCE;
		}
	}

	if (sc->bge_asicrev == BGE_ASICREV_BCM5703 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5704 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		dma_rw_ctl &= ~BGE_PCIDMARWCTL_MINDMA;
	pci_write_config(sc->bge_dev, BGE_PCI_DMA_RW_CTL, dma_rw_ctl, 4);

	/*
	 * Set up general mode register.
	 */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS|
	    BGE_MODECTL_MAC_ATTN_INTR|BGE_MODECTL_HOST_SEND_BDS|
	    BGE_MODECTL_TX_NO_PHDR_CSUM);

	/*
	 * Disable memory write invalidate.  Apparently it is not supported
	 * properly by these devices.
	 */
	PCI_CLRBIT(sc->bge_dev, BGE_PCI_CMD, PCIM_CMD_MWIEN, 4);

#ifdef __brokenalpha__
	/*
	 * Must insure that we do not cross an 8K (bytes) boundary
	 * for DMA reads.  Our highest limit is 1K bytes.  This is a
	 * restriction on some ALPHA platforms with early revision
	 * 21174 PCI chipsets, such as the AlphaPC 164lx
	 */
	PCI_SETBIT(sc->bge_dev, BGE_PCI_DMA_RW_CTL,
	    BGE_PCI_READ_BNDRY_1024BYTES, 4);
#endif

	/* Set the timer prescaler (always 66Mhz) */
	CSR_WRITE_4(sc, BGE_MISC_CFG, 65 << 1/*BGE_32BITTIME_66MHZ*/);

	return(0);
}

static int
bge_blockinit(sc)
	struct bge_softc *sc;
{
	struct bge_rcb *rcb;
	bus_size_t vrcb;
	bge_hostaddr taddr;
	int i;

	/*
	 * Initialize the memory window pointer register so that
	 * we can access the first 32K of internal NIC RAM. This will
	 * allow us to set up the TX send ring RCBs and the RX return
	 * ring RCBs, plus other things which live in NIC memory.
	 */
	CSR_WRITE_4(sc, BGE_PCI_MEMWIN_BASEADDR, 0);

	/* Note: the BCM5704 has a smaller mbuf space than other chips. */

	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		/* Configure mbuf memory pool */
		if (sc->bge_extram) {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR,
			    BGE_EXT_SSRAM);
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
			else
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);
		} else {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR,
			    BGE_BUFFPOOL_1);
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
			else
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);
		}

		/* Configure DMA resource pool */
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_BASEADDR,
		    BGE_DMA_DESCRIPTORS);
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LEN, 0x2000);
	}

	/* Configure mbuf pool watermarks */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x10);
	} else {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x50);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x20);
	}
	CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);

	/* Configure DMA resource watermarks */
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LOWAT, 5);
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_HIWAT, 10);

	/* Enable buffer manager */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_BMAN_MODE,
		    BGE_BMANMODE_ENABLE|BGE_BMANMODE_LOMBUF_ATTN);

		/* Poll for buffer manager start indication */
		for (i = 0; i < BGE_TIMEOUT; i++) {
			if (CSR_READ_4(sc, BGE_BMAN_MODE) & BGE_BMANMODE_ENABLE)
				break;
			DELAY(10);
		}

		if (i == BGE_TIMEOUT) {
			device_printf(sc->bge_dev,
			    "buffer manager failed to start\n");
			return(ENXIO);
		}
	}

	/* Enable flow-through queues */
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	/* Wait until queue initialization is complete */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (CSR_READ_4(sc, BGE_FTQ_RESET) == 0)
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev, "flow-through queue init failed\n");
		return(ENXIO);
	}

	/* Initialize the standard RX ring control block */
	rcb = &sc->bge_ldata.bge_info.bge_std_rx_rcb;
	rcb->bge_hostaddr.bge_addr_lo =
	    BGE_ADDR_LO(sc->bge_ldata.bge_rx_std_ring_paddr);
	rcb->bge_hostaddr.bge_addr_hi =
	    BGE_ADDR_HI(sc->bge_ldata.bge_rx_std_ring_paddr);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_PREREAD);
	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(512, 0);
	else
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(BGE_MAX_FRAMELEN, 0);
	if (sc->bge_extram)
		rcb->bge_nicaddr = BGE_EXT_STD_RX_RINGS;
	else
		rcb->bge_nicaddr = BGE_STD_RX_RINGS;
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_HI, rcb->bge_hostaddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_LO, rcb->bge_hostaddr.bge_addr_lo);

	CSR_WRITE_4(sc, BGE_RX_STD_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_NICADDR, rcb->bge_nicaddr);

	/*
	 * Initialize the jumbo RX ring control block
	 * We set the 'ring disabled' bit in the flags
	 * field until we're actually ready to start
	 * using this ring (i.e. once we set the MTU
	 * high enough to require it).
	 */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		rcb = &sc->bge_ldata.bge_info.bge_jumbo_rx_rcb;

		rcb->bge_hostaddr.bge_addr_lo =
		    BGE_ADDR_LO(sc->bge_ldata.bge_rx_jumbo_ring_paddr);
		rcb->bge_hostaddr.bge_addr_hi =
		    BGE_ADDR_HI(sc->bge_ldata.bge_rx_jumbo_ring_paddr);
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    BUS_DMASYNC_PREREAD);
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0,
		    BGE_RCB_FLAG_USE_EXT_RX_BD|BGE_RCB_FLAG_RING_DISABLED);
		if (sc->bge_extram)
			rcb->bge_nicaddr = BGE_EXT_JUMBO_RX_RINGS;
		else
			rcb->bge_nicaddr = BGE_JUMBO_RX_RINGS;
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_HI,
		    rcb->bge_hostaddr.bge_addr_hi);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_LO,
		    rcb->bge_hostaddr.bge_addr_lo);

		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_NICADDR, rcb->bge_nicaddr);

		/* Set up dummy disabled mini ring RCB */
		rcb = &sc->bge_ldata.bge_info.bge_mini_rx_rcb;
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED);
		CSR_WRITE_4(sc, BGE_RX_MINI_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
	}

	/*
	 * Set the BD ring replentish thresholds. The recommended
	 * values are 1/8th the number of descriptors allocated to
	 * each ring.
	 */
	CSR_WRITE_4(sc, BGE_RBDI_STD_REPL_THRESH, BGE_STD_RX_RING_CNT/8);
	CSR_WRITE_4(sc, BGE_RBDI_JUMBO_REPL_THRESH, BGE_JUMBO_RX_RING_CNT/8);

	/*
	 * Disable all unused send rings by setting the 'ring disabled'
	 * bit in the flags field of all the TX send ring control blocks.
	 * These are located in NIC memory.
	 */
	vrcb = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	for (i = 0; i < BGE_TX_RINGS_EXTSSRAM_MAX; i++) {
		RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, vrcb, bge_nicaddr, 0);
		vrcb += sizeof(struct bge_rcb);
	}

	/* Configure TX RCB 0 (we use only the first ring) */
	vrcb = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	BGE_HOSTADDR(taddr, sc->bge_ldata.bge_tx_ring_paddr);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, vrcb, bge_nicaddr,
	    BGE_NIC_TXRING_ADDR(0, BGE_TX_RING_CNT));
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(BGE_TX_RING_CNT, 0));

	/* Disable all unused RX return rings */
	vrcb = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	for (i = 0; i < BGE_RX_RINGS_MAX; i++) {
		RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_hi, 0);
		RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_lo, 0);
		RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt,
		    BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, vrcb, bge_nicaddr, 0);
		CSR_WRITE_4(sc, BGE_MBX_RX_CONS0_LO +
		    (i * (sizeof(u_int64_t))), 0);
		vrcb += sizeof(struct bge_rcb);
	}

	/* Initialize RX ring indexes */
	CSR_WRITE_4(sc, BGE_MBX_RX_STD_PROD_LO, 0);
	CSR_WRITE_4(sc, BGE_MBX_RX_JUMBO_PROD_LO, 0);
	CSR_WRITE_4(sc, BGE_MBX_RX_MINI_PROD_LO, 0);

	/*
	 * Set up RX return ring 0
	 * Note that the NIC address for RX return rings is 0x00000000.
	 * The return rings live entirely within the host, so the
	 * nicaddr field in the RCB isn't used.
	 */
	vrcb = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	BGE_HOSTADDR(taddr, sc->bge_ldata.bge_rx_return_ring_paddr);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, vrcb, bge_nicaddr, 0x00000000);
	RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
	    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt, 0));	

	/* Set random backoff seed for TX */
	CSR_WRITE_4(sc, BGE_TX_RANDOM_BACKOFF,
	    IFP2ENADDR(sc->bge_ifp)[0] + IFP2ENADDR(sc->bge_ifp)[1] +
	    IFP2ENADDR(sc->bge_ifp)[2] + IFP2ENADDR(sc->bge_ifp)[3] +
	    IFP2ENADDR(sc->bge_ifp)[4] + IFP2ENADDR(sc->bge_ifp)[5] +
	    BGE_TX_BACKOFF_SEED_MASK);

	/* Set inter-packet gap */
	CSR_WRITE_4(sc, BGE_TX_LENGTHS, 0x2620);

	/*
	 * Specify which ring to use for packets that don't match
	 * any RX rules.
	 */
	CSR_WRITE_4(sc, BGE_RX_RULES_CFG, 0x08);

	/*
	 * Configure number of RX lists. One interrupt distribution
	 * list, sixteen active lists, one bad frames class.
	 */
	CSR_WRITE_4(sc, BGE_RXLP_CFG, 0x181);

	/* Inialize RX list placement stats mask. */
	CSR_WRITE_4(sc, BGE_RXLP_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_RXLP_STATS_CTL, 0x1);

	/* Disable host coalescing until we get it set up */
	CSR_WRITE_4(sc, BGE_HCC_MODE, 0x00000000);

	/* Poll to make sure it's shut down. */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, BGE_HCC_MODE) & BGE_HCCMODE_ENABLE))
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev,
		    "host coalescing engine failed to idle\n");
		return(ENXIO);
	}

	/* Set up host coalescing defaults */
	CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS, sc->bge_rx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS, sc->bge_tx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS, sc->bge_rx_max_coal_bds);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS, sc->bge_tx_max_coal_bds);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS_INT, 0);
		CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS_INT, 0);
	}
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 0);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 0);

	/* Set up address of statistics block */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_HI,
		    BGE_ADDR_HI(sc->bge_ldata.bge_stats_paddr));
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_LO,
		    BGE_ADDR_LO(sc->bge_ldata.bge_stats_paddr));
		CSR_WRITE_4(sc, BGE_HCC_STATS_BASEADDR, BGE_STATS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_BASEADDR, BGE_STATUS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATS_TICKS, sc->bge_stat_ticks);
	}

	/* Set up address of status block */
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_HI,
	    BGE_ADDR_HI(sc->bge_ldata.bge_status_block_paddr));
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_LO,
	    BGE_ADDR_LO(sc->bge_ldata.bge_status_block_paddr));
	sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx = 0;
	sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx = 0;

	/* Turn on host coalescing state machine */
	CSR_WRITE_4(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);

	/* Turn on RX BD completion state machine and enable attentions */
	CSR_WRITE_4(sc, BGE_RBDC_MODE,
	    BGE_RBDCMODE_ENABLE|BGE_RBDCMODE_ATTN);

	/* Turn on RX list placement state machine */
	CSR_WRITE_4(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);

	/* Turn on RX list selector state machine. */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);

	/* Turn on DMA, clear stats */
	CSR_WRITE_4(sc, BGE_MAC_MODE, BGE_MACMODE_TXDMA_ENB|
	    BGE_MACMODE_RXDMA_ENB|BGE_MACMODE_RX_STATS_CLEAR|
	    BGE_MACMODE_TX_STATS_CLEAR|BGE_MACMODE_RX_STATS_ENB|
	    BGE_MACMODE_TX_STATS_ENB|BGE_MACMODE_FRMHDR_DMA_ENB|
	    (sc->bge_tbi ? BGE_PORTMODE_TBI : BGE_PORTMODE_MII));

	/* Set misc. local control, enable interrupts on attentions */
	CSR_WRITE_4(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_ONATTN);

#ifdef notdef
	/* Assert GPIO pins for PHY reset */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUT0|
	    BGE_MLC_MISCIO_OUT1|BGE_MLC_MISCIO_OUT2);
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUTEN0|
	    BGE_MLC_MISCIO_OUTEN1|BGE_MLC_MISCIO_OUTEN2);
#endif

	/* Turn on DMA completion state machine */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);

	/* Turn on write DMA state machine */
	CSR_WRITE_4(sc, BGE_WDMA_MODE,
	    BGE_WDMAMODE_ENABLE|BGE_WDMAMODE_ALL_ATTNS);

	/* Turn on read DMA state machine */
	CSR_WRITE_4(sc, BGE_RDMA_MODE,
	    BGE_RDMAMODE_ENABLE|BGE_RDMAMODE_ALL_ATTNS);

	/* Turn on RX data completion state machine */
	CSR_WRITE_4(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);

	/* Turn on RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);

	/* Turn on RX data and RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RDBDI_MODE, BGE_RDBDIMODE_ENABLE);

	/* Turn on Mbuf cluster free state machine */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	/* Turn on send BD completion state machine */
	CSR_WRITE_4(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/* Turn on send data completion state machine */
	CSR_WRITE_4(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);

	/* Turn on send data initiator state machine */
	CSR_WRITE_4(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);

	/* Turn on send BD initiator state machine */
	CSR_WRITE_4(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);

	/* Turn on send BD selector state machine */
	CSR_WRITE_4(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);

	CSR_WRITE_4(sc, BGE_SDI_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_SDI_STATS_CTL,
	    BGE_SDISTATSCTL_ENABLE|BGE_SDISTATSCTL_FASTER);

	/* ack/clear link change events */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);
	CSR_WRITE_4(sc, BGE_MI_STS, 0);

	/* Enable PHY auto polling (for MII/GMII only) */
	if (sc->bge_tbi) {
		CSR_WRITE_4(sc, BGE_MI_STS, BGE_MISTS_LINK);
	} else {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL|10<<16);
		if (sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
		    sc->bge_chipid != BGE_CHIPID_BCM5700_B1)
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
	}

	/*
	 * Clear any pending link state attention.
	 * Otherwise some link state change events may be lost until attention
	 * is cleared by bge_intr() -> bge_link_upd() sequence.
	 * It's not necessary on newer BCM chips - perhaps enabling link
	 * state change attentions implies clearing pending attention.
	 */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);

	/* Enable link state change attentions. */
	BGE_SETBIT(sc, BGE_MAC_EVT_ENB, BGE_EVTENB_LINK_CHANGED);

	return(0);
}

/*
 * Probe for a Broadcom chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match. Note
 * that since the Broadcom controller contains VPD support, we
 * can get the device name string from the controller itself instead
 * of the compiled-in string. This is a little slow, but it guarantees
 * we'll always announce the right product name.
 */
static int
bge_probe(dev)
	device_t dev;
{
	struct bge_type *t;
	struct bge_softc *sc;
	char *descbuf;

	t = bge_devs;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(struct bge_softc));
	sc->bge_dev = dev;

	while(t->bge_name != NULL) {
		if ((pci_get_vendor(dev) == t->bge_vid) &&
		    (pci_get_device(dev) == t->bge_did)) {
#ifdef notdef
			bge_vpd_read(sc);
			device_set_desc(dev, sc->bge_vpd_prodname);
#endif
			descbuf = malloc(BGE_DEVDESC_MAX, M_TEMP, M_NOWAIT);
			if (descbuf == NULL)
				return(ENOMEM);
			snprintf(descbuf, BGE_DEVDESC_MAX,
			    "%s, ASIC rev. %#04x", t->bge_name,
			    pci_read_config(dev, BGE_PCI_MISC_CTL, 4) >> 16);
			device_set_desc_copy(dev, descbuf);
			if (pci_get_subvendor(dev) == DELL_VENDORID)
				sc->bge_no_3_led = 1;
			free(descbuf, M_TEMP);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

static void
bge_dma_free(sc)
	struct bge_softc *sc;
{
	int i;


	/* Destroy DMA maps for RX buffers */

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i]);
	}

	/* Destroy DMA maps for jumbo RX buffers */

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
	}

	/* Destroy DMA maps for TX buffers */

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i]);
	}

	if (sc->bge_cdata.bge_mtag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_mtag);


	/* Destroy standard RX ring */

	if (sc->bge_cdata.bge_rx_std_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_cdata.bge_rx_std_ring_map);
	if (sc->bge_cdata.bge_rx_std_ring_map && sc->bge_ldata.bge_rx_std_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_ldata.bge_rx_std_ring,
		    sc->bge_cdata.bge_rx_std_ring_map);

	if (sc->bge_cdata.bge_rx_std_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_std_ring_tag);

	/* Destroy jumbo RX ring */

	if (sc->bge_cdata.bge_rx_jumbo_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);

	if (sc->bge_cdata.bge_rx_jumbo_ring_map &&
	    sc->bge_ldata.bge_rx_jumbo_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_ldata.bge_rx_jumbo_ring,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);

	if (sc->bge_cdata.bge_rx_jumbo_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_jumbo_ring_tag);

	/* Destroy RX return ring */

	if (sc->bge_cdata.bge_rx_return_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_cdata.bge_rx_return_ring_map);

	if (sc->bge_cdata.bge_rx_return_ring_map &&
	    sc->bge_ldata.bge_rx_return_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_ldata.bge_rx_return_ring,
		    sc->bge_cdata.bge_rx_return_ring_map);

	if (sc->bge_cdata.bge_rx_return_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_return_ring_tag);

	/* Destroy TX ring */

	if (sc->bge_cdata.bge_tx_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_cdata.bge_tx_ring_map);

	if (sc->bge_cdata.bge_tx_ring_map && sc->bge_ldata.bge_tx_ring)
		bus_dmamem_free(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_ldata.bge_tx_ring,
		    sc->bge_cdata.bge_tx_ring_map);

	if (sc->bge_cdata.bge_tx_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_tx_ring_tag);

	/* Destroy status block */

	if (sc->bge_cdata.bge_status_map)
		bus_dmamap_unload(sc->bge_cdata.bge_status_tag,
		    sc->bge_cdata.bge_status_map);

	if (sc->bge_cdata.bge_status_map && sc->bge_ldata.bge_status_block)
		bus_dmamem_free(sc->bge_cdata.bge_status_tag,
		    sc->bge_ldata.bge_status_block,
		    sc->bge_cdata.bge_status_map);

	if (sc->bge_cdata.bge_status_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_status_tag);

	/* Destroy statistics block */

	if (sc->bge_cdata.bge_stats_map)
		bus_dmamap_unload(sc->bge_cdata.bge_stats_tag,
		    sc->bge_cdata.bge_stats_map);

	if (sc->bge_cdata.bge_stats_map && sc->bge_ldata.bge_stats)
		bus_dmamem_free(sc->bge_cdata.bge_stats_tag,
		    sc->bge_ldata.bge_stats,
		    sc->bge_cdata.bge_stats_map);

	if (sc->bge_cdata.bge_stats_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_stats_tag);

	/* Destroy the parent tag */

	if (sc->bge_cdata.bge_parent_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_parent_tag);

	return;
}

static int
bge_dma_alloc(dev)
	device_t dev;
{
	struct bge_softc *sc;
	int i, error;
	struct bge_dmamap_arg ctx;

	sc = device_get_softc(dev);

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
	error = bus_dma_tag_create(NULL,	/* parent */
			PAGE_SIZE, 0,		/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, BGE_NSEG_NEW,	/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->bge_cdata.bge_parent_tag);

	if (error != 0) {
		device_printf(sc->bge_dev,
		    "could not allocate parent dma tag\n");
		return (ENOMEM);
	}

	/*
	 * Create tag for RX mbufs.
	 */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag, 1,
	    0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, MCLBYTES * BGE_NSEG_NEW, BGE_NSEG_NEW, MCLBYTES,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &sc->bge_cdata.bge_mtag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Create DMA maps for RX buffers */

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		error = bus_dmamap_create(sc->bge_cdata.bge_mtag, 0,
			    &sc->bge_cdata.bge_rx_std_dmamap[i]);
		if (error) {
			device_printf(sc->bge_dev,
			    "can't create DMA map for RX\n");
			return(ENOMEM);
		}
	}

	/* Create DMA maps for TX buffers */

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		error = bus_dmamap_create(sc->bge_cdata.bge_mtag, 0,
			    &sc->bge_cdata.bge_tx_dmamap[i]);
		if (error) {
			device_printf(sc->bge_dev,
			    "can't create DMA map for RX\n");
			return(ENOMEM);
		}
	}

	/* Create tag for standard RX ring */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STD_RX_RING_SZ, 1, BGE_STD_RX_RING_SZ, 0,
	    NULL, NULL, &sc->bge_cdata.bge_rx_std_ring_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for standard RX ring */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_std_ring_tag,
	    (void **)&sc->bge_ldata.bge_rx_std_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_rx_std_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_rx_std_ring, BGE_STD_RX_RING_SZ);

	/* Load the address of the standard RX ring */

	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, sc->bge_ldata.bge_rx_std_ring,
	    BGE_STD_RX_RING_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_rx_std_ring_paddr = ctx.bge_busaddr;

	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {

		/*
		 * Create tag for jumbo mbufs.
		 * This is really a bit of a kludge. We allocate a special
		 * jumbo buffer pool which (thanks to the way our DMA
		 * memory allocation works) will consist of contiguous
		 * pages. This means that even though a jumbo buffer might
		 * be larger than a page size, we don't really need to
		 * map it into more than one DMA segment. However, the
		 * default mbuf tag will result in multi-segment mappings,
		 * so we have to create a special jumbo mbuf tag that
		 * lets us get away with mapping the jumbo buffers as
		 * a single segment. I think eventually the driver should
		 * be changed so that it uses ordinary mbufs and cluster
		 * buffers, i.e. jumbo frames can span multiple DMA
		 * descriptors. But that's a project for another day.
		 */

		error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
		    1, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
		    NULL, MJUM9BYTES, BGE_NSEG_JUMBO, PAGE_SIZE,
		    0, NULL, NULL, &sc->bge_cdata.bge_mtag_jumbo);

		if (error) {
			device_printf(sc->bge_dev,
			    "could not allocate dma tag\n");
			return (ENOMEM);
		}

		/* Create tag for jumbo RX ring */
		error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
		    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
		    NULL, BGE_JUMBO_RX_RING_SZ, 1, BGE_JUMBO_RX_RING_SZ, 0,
		    NULL, NULL, &sc->bge_cdata.bge_rx_jumbo_ring_tag);

		if (error) {
			device_printf(sc->bge_dev,
			    "could not allocate dma tag\n");
			return (ENOMEM);
		}

		/* Allocate DMA'able memory for jumbo RX ring */
		error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    (void **)&sc->bge_ldata.bge_rx_jumbo_ring,
		    BUS_DMA_NOWAIT | BUS_DMA_ZERO,
		    &sc->bge_cdata.bge_rx_jumbo_ring_map);
		if (error)
			return (ENOMEM);

		/* Load the address of the jumbo RX ring */
		ctx.bge_maxsegs = 1;
		ctx.sc = sc;

		error = bus_dmamap_load(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    sc->bge_ldata.bge_rx_jumbo_ring, BGE_JUMBO_RX_RING_SZ,
		    bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

		if (error)
			return (ENOMEM);

		sc->bge_ldata.bge_rx_jumbo_ring_paddr = ctx.bge_busaddr;

		/* Create DMA maps for jumbo RX buffers */

		for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
			error = bus_dmamap_create(sc->bge_cdata.bge_mtag_jumbo,
				    0, &sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
			if (error) {
				device_printf(sc->bge_dev,
				    "can't create DMA map for RX\n");
				return(ENOMEM);
			}
		}

	}

	/* Create tag for RX return ring */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_RX_RTN_RING_SZ(sc), 1, BGE_RX_RTN_RING_SZ(sc), 0,
	    NULL, NULL, &sc->bge_cdata.bge_rx_return_ring_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for RX return ring */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_return_ring_tag,
	    (void **)&sc->bge_ldata.bge_rx_return_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_rx_return_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_rx_return_ring,
	    BGE_RX_RTN_RING_SZ(sc));

	/* Load the address of the RX return ring */

	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map,
	    sc->bge_ldata.bge_rx_return_ring, BGE_RX_RTN_RING_SZ(sc),
	    bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_rx_return_ring_paddr = ctx.bge_busaddr;

	/* Create tag for TX ring */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_TX_RING_SZ, 1, BGE_TX_RING_SZ, 0, NULL, NULL,
	    &sc->bge_cdata.bge_tx_ring_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for TX ring */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_tx_ring_tag,
	    (void **)&sc->bge_ldata.bge_tx_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_tx_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_tx_ring, BGE_TX_RING_SZ);

	/* Load the address of the TX ring */

	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_tx_ring_tag,
	    sc->bge_cdata.bge_tx_ring_map, sc->bge_ldata.bge_tx_ring,
	    BGE_TX_RING_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_tx_ring_paddr = ctx.bge_busaddr;

	/* Create tag for status block */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STATUS_BLK_SZ, 1, BGE_STATUS_BLK_SZ, 0,
	    NULL, NULL, &sc->bge_cdata.bge_status_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for status block */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_status_tag,
	    (void **)&sc->bge_ldata.bge_status_block, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_status_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_status_block, BGE_STATUS_BLK_SZ);

	/* Load the address of the status block */

	ctx.sc = sc;
	ctx.bge_maxsegs = 1;

	error = bus_dmamap_load(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, sc->bge_ldata.bge_status_block,
	    BGE_STATUS_BLK_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_status_block_paddr = ctx.bge_busaddr;

	/* Create tag for statistics block */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STATS_SZ, 1, BGE_STATS_SZ, 0, NULL, NULL,
	    &sc->bge_cdata.bge_stats_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for statistics block */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_stats_tag,
	    (void **)&sc->bge_ldata.bge_stats, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_stats_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_stats, BGE_STATS_SZ);

	/* Load the address of the statstics block */

	ctx.sc = sc;
	ctx.bge_maxsegs = 1;

	error = bus_dmamap_load(sc->bge_cdata.bge_stats_tag,
	    sc->bge_cdata.bge_stats_map, sc->bge_ldata.bge_stats,
	    BGE_STATS_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_stats_paddr = ctx.bge_busaddr;

	return(0);
}

static int
bge_attach(dev)
	device_t dev;
{
	struct ifnet *ifp;
	struct bge_softc *sc;
	u_int32_t hwcfg = 0;
	u_int32_t mac_tmp = 0;
	u_char eaddr[6];
	int error = 0, rid;

	sc = device_get_softc(dev);
	sc->bge_dev = dev;

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = BGE_PCI_BAR0;
	sc->bge_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE|PCI_RF_DENSE);

	if (sc->bge_res == NULL) {
		device_printf (sc->bge_dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->bge_btag = rman_get_bustag(sc->bge_res);
	sc->bge_bhandle = rman_get_bushandle(sc->bge_res);

	/* Allocate interrupt */
	rid = 0;

	sc->bge_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->bge_irq == NULL) {
		device_printf(sc->bge_dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	BGE_LOCK_INIT(sc, device_get_nameunit(dev));

	/* Save ASIC rev. */

	sc->bge_chipid =
	    pci_read_config(dev, BGE_PCI_MISC_CTL, 4) &
	    BGE_PCIMISCCTL_ASICREV;
	sc->bge_asicrev = BGE_ASICREV(sc->bge_chipid);
	sc->bge_chiprev = BGE_CHIPREV(sc->bge_chipid);

	/*
	 * Treat the 5714 and the 5752 like the 5750 until we have more info
	 * on this chip.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5714 || 
            sc->bge_asicrev == BGE_ASICREV_BCM5752)
		sc->bge_asicrev = BGE_ASICREV_BCM5750;

	/*
	 * XXX: Broadcom Linux driver.  Not in specs or eratta.
	 * PCI-Express?
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5750) {
		u_int32_t v;

		v = pci_read_config(dev, BGE_PCI_MSI_CAPID, 4);
		if (((v >> 8) & 0xff) == BGE_PCIE_CAPID_REG) {
			v = pci_read_config(dev, BGE_PCIE_CAPID_REG, 4);
			if ((v & 0xff) == BGE_PCIE_CAPID)
				sc->bge_pcie = 1;
		}
	}

	/* Try to reset the chip. */
	bge_reset(sc);

	if (bge_chipinit(sc)) {
		device_printf(sc->bge_dev, "chip initialization failed\n");
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Get station address from the EEPROM.
	 */
	mac_tmp = bge_readmem_ind(sc, 0x0c14);
	if ((mac_tmp >> 16) == 0x484b) {
		eaddr[0] = (u_char)(mac_tmp >> 8);
		eaddr[1] = (u_char)mac_tmp;
		mac_tmp = bge_readmem_ind(sc, 0x0c18);
		eaddr[2] = (u_char)(mac_tmp >> 24);
		eaddr[3] = (u_char)(mac_tmp >> 16);
		eaddr[4] = (u_char)(mac_tmp >> 8);
		eaddr[5] = (u_char)mac_tmp;
	} else if (bge_read_eeprom(sc, eaddr,
	    BGE_EE_MAC_OFFSET + 2, ETHER_ADDR_LEN)) {
		device_printf(sc->bge_dev, "failed to read station address\n");
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	/* 5705 limits RX return ring to 512 entries. */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT_5705;
	else
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT;

	if (bge_dma_alloc(dev)) {
		device_printf(sc->bge_dev,
		    "failed to allocate DMA resources\n");
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	/* Set default tuneable values. */
	sc->bge_stat_ticks = BGE_TICKS_PER_SEC;
	sc->bge_rx_coal_ticks = 150;
	sc->bge_tx_coal_ticks = 150;
	sc->bge_rx_max_coal_bds = 64;
	sc->bge_tx_max_coal_bds = 128;

	/* Set up ifnet structure */
	ifp = sc->bge_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->bge_dev, "failed to if_alloc()\n");
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bge_ioctl;
	ifp->if_start = bge_start;
	ifp->if_watchdog = bge_watchdog;
	ifp->if_init = bge_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_drv_maxlen = BGE_TX_RING_CNT - 1;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_hwassist = BGE_CSUM_FEATURES;
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_VLAN_HWTAGGING |
	    IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

        /*
	 * 5700 B0 chips do not support checksumming correctly due
	 * to hardware bugs.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5700_B0) {
		ifp->if_capabilities &= ~IFCAP_HWCSUM;
		ifp->if_capenable &= IFCAP_HWCSUM;
		ifp->if_hwassist = 0;
	}

	/*
	 * Figure out what sort of media we have by checking the
	 * hardware config word in the first 32k of NIC internal memory,
	 * or fall back to examining the EEPROM if necessary.
	 * Note: on some BCM5700 cards, this value appears to be unset.
	 * If that's the case, we have to rely on identifying the NIC
	 * by its PCI subsystem ID, as we do below for the SysKonnect
	 * SK-9D41.
	 */
	if (bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_SIG) == BGE_MAGIC_NUMBER)
		hwcfg = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_NICCFG);
	else {
		if (bge_read_eeprom(sc, (caddr_t)&hwcfg, BGE_EE_HWCFG_OFFSET,
		    sizeof(hwcfg))) {
			device_printf(sc->bge_dev, "failed to read EEPROM\n");
			bge_release_resources(sc);
			error = ENXIO;
			goto fail;
		}
		hwcfg = ntohl(hwcfg);
	}

	if ((hwcfg & BGE_HWCFG_MEDIA) == BGE_MEDIA_FIBER)
		sc->bge_tbi = 1;

	/* The SysKonnect SK-9D41 is a 1000baseSX card. */
	if ((pci_read_config(dev, BGE_PCI_SUBSYS, 4) >> 16) == SK_SUBSYSID_9D41)
		sc->bge_tbi = 1;

	if (sc->bge_tbi) {
		ifmedia_init(&sc->bge_ifmedia, IFM_IMASK,
		    bge_ifmedia_upd, bge_ifmedia_sts);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->bge_ifmedia, IFM_ETHER|IFM_AUTO);
		sc->bge_ifmedia.ifm_media = sc->bge_ifmedia.ifm_cur->ifm_media;
	} else {
		/*
		 * Do transceiver setup.
		 */
		if (mii_phy_probe(dev, &sc->bge_miibus,
		    bge_ifmedia_upd, bge_ifmedia_sts)) {
			device_printf(sc->bge_dev, "MII without any PHY!\n");
			bge_release_resources(sc);
			error = ENXIO;
			goto fail;
		}
	}

	/*
	 * When using the BCM5701 in PCI-X mode, data corruption has
	 * been observed in the first few bytes of some received packets.
	 * Aligning the packet buffer in memory eliminates the corruption.
	 * Unfortunately, this misaligns the packet payloads.  On platforms
	 * which do not support unaligned accesses, we will realign the
	 * payloads by copying the received packets.
	 */
	switch (sc->bge_chipid) {
	case BGE_CHIPID_BCM5701_A0:
	case BGE_CHIPID_BCM5701_B0:
	case BGE_CHIPID_BCM5701_B2:
	case BGE_CHIPID_BCM5701_B5:
		/* If in PCI-X mode, work around the alignment bug. */
		if ((pci_read_config(dev, BGE_PCI_PCISTATE, 4) &
		    (BGE_PCISTATE_PCI_BUSMODE | BGE_PCISTATE_PCI_BUSSPEED)) ==
		    BGE_PCISTATE_PCI_BUSSPEED)
			sc->bge_rx_alignment_bug = 1;
		break;
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
	callout_init(&sc->bge_stat_ch, CALLOUT_MPSAFE);

	/*
	 * Hookup IRQ last.
	 */
	error = bus_setup_intr(dev, sc->bge_irq, INTR_TYPE_NET | INTR_MPSAFE,
	   bge_intr, sc, &sc->bge_intrhand);

	if (error) {
		bge_detach(dev);
		device_printf(sc->bge_dev, "couldn't set up irq\n");
	}

fail:
	return(error);
}

static int
bge_detach(dev)
	device_t dev;
{
	struct bge_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->bge_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	BGE_LOCK(sc);
	bge_stop(sc);
	bge_reset(sc);
	BGE_UNLOCK(sc);

	ether_ifdetach(ifp);

	if (sc->bge_tbi) {
		ifmedia_removeall(&sc->bge_ifmedia);
	} else {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->bge_miibus);
	}

	bge_release_resources(sc);

	return(0);
}

static void
bge_release_resources(sc)
	struct bge_softc *sc;
{
	device_t dev;

	dev = sc->bge_dev;

	if (sc->bge_vpd_prodname != NULL)
		free(sc->bge_vpd_prodname, M_DEVBUF);

	if (sc->bge_vpd_readonly != NULL)
		free(sc->bge_vpd_readonly, M_DEVBUF);

	if (sc->bge_intrhand != NULL)
		bus_teardown_intr(dev, sc->bge_irq, sc->bge_intrhand);

	if (sc->bge_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->bge_irq);

	if (sc->bge_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    BGE_PCI_BAR0, sc->bge_res);

	if (sc->bge_ifp != NULL)
		if_free(sc->bge_ifp);

	bge_dma_free(sc);

	if (mtx_initialized(&sc->bge_mtx))	/* XXX */
		BGE_LOCK_DESTROY(sc);

	return;
}

static void
bge_reset(sc)
	struct bge_softc *sc;
{
	device_t dev;
	u_int32_t cachesize, command, pcistate, reset;
	int i, val = 0;

	dev = sc->bge_dev;

	/* Save some important PCI state. */
	cachesize = pci_read_config(dev, BGE_PCI_CACHESZ, 4);
	command = pci_read_config(dev, BGE_PCI_CMD, 4);
	pcistate = pci_read_config(dev, BGE_PCI_PCISTATE, 4);

	pci_write_config(dev, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS|BGE_PCIMISCCTL_MASK_PCI_INTR|
	BGE_HIF_SWAP_OPTIONS|BGE_PCIMISCCTL_PCISTATE_RW, 4);

	reset = BGE_MISCCFG_RESET_CORE_CLOCKS|(65<<1);

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_pcie) {
		if (CSR_READ_4(sc, 0x7e2c) == 0x60)	/* PCIE 1.0 */
			CSR_WRITE_4(sc, 0x7e2c, 0x20);
		if (sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
			/* Prevent PCIE link training during global reset */
			CSR_WRITE_4(sc, BGE_MISC_CFG, (1<<29));
			reset |= (1<<29);
		}
	}

	/* Issue global reset */
	bge_writereg_ind(sc, BGE_MISC_CFG, reset);

	DELAY(1000);

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_pcie) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5750_A0) {
			uint32_t v;

			DELAY(500000); /* wait for link training to complete */
			v = pci_read_config(dev, 0xc4, 4);
			pci_write_config(dev, 0xc4, v | (1<<15), 4);
		}
		/* Set PCIE max payload size and clear error status. */
		pci_write_config(dev, 0xd8, 0xf5000, 4);
	}

	/* Reset some of the PCI state that got zapped by reset */
	pci_write_config(dev, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS|BGE_PCIMISCCTL_MASK_PCI_INTR|
	    BGE_HIF_SWAP_OPTIONS|BGE_PCIMISCCTL_PCISTATE_RW, 4);
	pci_write_config(dev, BGE_PCI_CACHESZ, cachesize, 4);
	pci_write_config(dev, BGE_PCI_CMD, command, 4);
	bge_writereg_ind(sc, BGE_MISC_CFG, (65 << 1));

	/* Enable memory arbiter. */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);

	/*
	 * Prevent PXE restart: write a magic number to the
	 * general communications memory at 0xB50.
	 */
	bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM, BGE_MAGIC_NUMBER);
	/*
	 * Poll the value location we just wrote until
	 * we see the 1's complement of the magic number.
	 * This indicates that the firmware initialization
	 * is complete.
	 */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		val = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM);
		if (val == ~BGE_MAGIC_NUMBER)
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev, "firmware handshake timed out\n");
		return;
	}

	/*
	 * XXX Wait for the value of the PCISTATE register to
	 * return to its original pre-reset state. This is a
	 * fairly good indicator of reset completion. If we don't
	 * wait for the reset to fully complete, trying to read
	 * from the device's non-PCI registers may yield garbage
	 * results.
	 */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (pci_read_config(dev, BGE_PCI_PCISTATE, 4) == pcistate)
			break;
		DELAY(10);
	}

	/* Fix up byte swapping */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS|
	    BGE_MODECTL_BYTESWAP_DATA);

	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * The 5704 in TBI mode apparently needs some special
	 * adjustment to insure the SERDES drive level is set
	 * to 1.2V.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5704 && sc->bge_tbi) {
		uint32_t serdescfg;
		serdescfg = CSR_READ_4(sc, BGE_SERDES_CFG);
		serdescfg = (serdescfg & ~0xFFF) | 0x880;
		CSR_WRITE_4(sc, BGE_SERDES_CFG, serdescfg);
	}

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_pcie && sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
		uint32_t v;

		v = CSR_READ_4(sc, 0x7c00);
		CSR_WRITE_4(sc, 0x7c00, v | (1<<25));
	}
	DELAY(10000);

	return;
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle two possibilities here:
 * 1) the frame is from the jumbo receive ring
 * 2) the frame is from the standard receive ring
 */

static void
bge_rxeof(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	int stdcnt = 0, jumbocnt = 0;

	BGE_LOCK_ASSERT(sc);

	/* Nothing to do */
	if (sc->bge_rx_saved_considx ==
	    sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx)
		return;

	ifp = sc->bge_ifp;

	bus_dmamap_sync(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_POSTREAD);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    BUS_DMASYNC_POSTREAD);
	}

	while(sc->bge_rx_saved_considx !=
	    sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx) {
		struct bge_rx_bd	*cur_rx;
		u_int32_t		rxidx;
		struct ether_header	*eh;
		struct mbuf		*m = NULL;
		u_int16_t		vlan_tag = 0;
		int			have_tag = 0;

#ifdef DEVICE_POLLING
		if (ifp->if_capenable & IFCAP_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif

		cur_rx =
	    &sc->bge_ldata.bge_rx_return_ring[sc->bge_rx_saved_considx];

		rxidx = cur_rx->bge_idx;
		BGE_INC(sc->bge_rx_saved_considx, sc->bge_return_ring_cnt);

		if (cur_rx->bge_flags & BGE_RXBDFLAG_VLAN_TAG) {
			have_tag = 1;
			vlan_tag = cur_rx->bge_vlan_tag;
		}

		if (cur_rx->bge_flags & BGE_RXBDFLAG_JUMBO_RING) {
			BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
			bus_dmamap_sync(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[rxidx],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[rxidx]);
			m = sc->bge_cdata.bge_rx_jumbo_chain[rxidx];
			sc->bge_cdata.bge_rx_jumbo_chain[rxidx] = NULL;
			jumbocnt++;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				ifp->if_ierrors++;
				bge_newbuf_jumbo(sc, sc->bge_jumbo, m);
				continue;
			}
			if (bge_newbuf_jumbo(sc,
			    sc->bge_jumbo, NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				bge_newbuf_jumbo(sc, sc->bge_jumbo, m);
				continue;
			}
		} else {
			BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
			bus_dmamap_sync(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[rxidx],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[rxidx]);
			m = sc->bge_cdata.bge_rx_std_chain[rxidx];
			sc->bge_cdata.bge_rx_std_chain[rxidx] = NULL;
			stdcnt++;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m);
				continue;
			}
			if (bge_newbuf_std(sc, sc->bge_std,
			    NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m);
				continue;
			}
		}

		ifp->if_ipackets++;
#ifndef __NO_STRICT_ALIGNMENT
		/*
		 * For architectures with strict alignment we must make sure
		 * the payload is aligned.
		 */
		if (sc->bge_rx_alignment_bug) {
			bcopy(m->m_data, m->m_data + ETHER_ALIGN,
			    cur_rx->bge_len);
			m->m_data += ETHER_ALIGN;
		}
#endif
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.len = m->m_len = cur_rx->bge_len - ETHER_CRC_LEN;
		m->m_pkthdr.rcvif = ifp;

		if (ifp->if_capenable & IFCAP_RXCSUM) {
			if (cur_rx->bge_flags & BGE_RXBDFLAG_IP_CSUM) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				if ((cur_rx->bge_ip_csum ^ 0xffff) == 0)
					m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			}
			if (cur_rx->bge_flags & BGE_RXBDFLAG_TCP_UDP_CSUM &&
			    m->m_pkthdr.len >= ETHER_MIN_NOPAD) {
				m->m_pkthdr.csum_data =
				    cur_rx->bge_tcp_udp_csum;
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			}
		}

		/*
		 * If we received a packet with a vlan tag,
		 * attach that information to the packet.
		 */
		if (have_tag) {
			VLAN_INPUT_TAG_NEW(ifp, m, vlan_tag);
			if (m == NULL)
				continue;
		}

		BGE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		BGE_LOCK(sc);
	}

	if (stdcnt > 0)
		bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_PREWRITE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		if (jumbocnt > 0)
			bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
			    sc->bge_cdata.bge_rx_jumbo_ring_map,
			    BUS_DMASYNC_PREWRITE);
	}

	CSR_WRITE_4(sc, BGE_MBX_RX_CONS0_LO, sc->bge_rx_saved_considx);
	if (stdcnt)
		CSR_WRITE_4(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);
	if (jumbocnt)
		CSR_WRITE_4(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);
}

static void
bge_txeof(sc)
	struct bge_softc *sc;
{
	struct bge_tx_bd *cur_tx = NULL;
	struct ifnet *ifp;

	BGE_LOCK_ASSERT(sc);

	/* Nothing to do */
	if (sc->bge_tx_saved_considx ==
	    sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx)
		return;

	ifp = sc->bge_ifp;

	bus_dmamap_sync(sc->bge_cdata.bge_tx_ring_tag,
	    sc->bge_cdata.bge_tx_ring_map,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->bge_tx_saved_considx !=
	    sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx) {
		u_int32_t		idx = 0;

		idx = sc->bge_tx_saved_considx;
		cur_tx = &sc->bge_ldata.bge_tx_ring[idx];
		if (cur_tx->bge_flags & BGE_TXBDFLAG_END)
			ifp->if_opackets++;
		if (sc->bge_cdata.bge_tx_chain[idx] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[idx],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[idx]);
			m_freem(sc->bge_cdata.bge_tx_chain[idx]);
			sc->bge_cdata.bge_tx_chain[idx] = NULL;
		}
		sc->bge_txcnt--;
		BGE_INC(sc->bge_tx_saved_considx, BGE_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	if (cur_tx != NULL)
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

#ifdef DEVICE_POLLING
static void
bge_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct bge_softc *sc = ifp->if_softc;
	
	BGE_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		bge_poll_locked(ifp, cmd, count);
	BGE_UNLOCK(sc);
}

static void
bge_poll_locked(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct bge_softc *sc = ifp->if_softc;
	uint32_t statusword;

	BGE_LOCK_ASSERT(sc);

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_POSTREAD);

	statusword = atomic_readandclear_32(&sc->bge_ldata.bge_status_block->bge_status);

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_PREREAD);

	/* Note link event. It will be processed by POLL_AND_CHECK_STATUS cmd */
	if (statusword & BGE_STATFLAG_LINKSTATE_CHANGED)
		sc->bge_link_evt++;

	if (cmd == POLL_AND_CHECK_STATUS)
		if ((sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
		    sc->bge_chipid != BGE_CHIPID_BCM5700_B1) ||
		    sc->bge_link_evt || sc->bge_tbi)
			bge_link_upd(sc);

	sc->rxcycles = count;
	bge_rxeof(sc);
	bge_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bge_start_locked(ifp);
}
#endif /* DEVICE_POLLING */

static void
bge_intr(xsc)
	void *xsc;
{
	struct bge_softc *sc;
	struct ifnet *ifp;
	uint32_t statusword;

	sc = xsc;

	BGE_LOCK(sc);

	ifp = sc->bge_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		BGE_UNLOCK(sc);
		return;
	}
#endif

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_POSTREAD);

	statusword =
	    atomic_readandclear_32(&sc->bge_ldata.bge_status_block->bge_status);

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_PREREAD);

#ifdef notdef
	/* Avoid this for now -- checking this register is expensive. */
	/* Make sure this is really our interrupt. */
	if (!(CSR_READ_4(sc, BGE_MISC_LOCAL_CTL) & BGE_MLC_INTR_STATE))
		return;
#endif
	/* Ack interrupt and stop others from occuring. */
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 1);

	if ((sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5700_B1) ||
	    statusword & BGE_STATFLAG_LINKSTATE_CHANGED || sc->bge_link_evt)
		bge_link_upd(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/* Check RX return ring producer/consumer */
		bge_rxeof(sc);

		/* Check TX ring producer/consumer */
		bge_txeof(sc);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 0);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bge_start_locked(ifp);

	BGE_UNLOCK(sc);

	return;
}

static void
bge_tick_locked(sc)
	struct bge_softc *sc;
{
	struct mii_data *mii = NULL;

	BGE_LOCK_ASSERT(sc);

	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		bge_stats_update_regs(sc);
	else
		bge_stats_update(sc);

	if (!sc->bge_tbi) {
		mii = device_get_softc(sc->bge_miibus);
		mii_tick(mii);
	} else {
		/*
		 * Since in TBI mode auto-polling can't be used we should poll
		 * link status manually. Here we register pending link event
		 * and trigger interrupt.
		 */
#ifdef DEVICE_POLLING
		/* In polling mode we poll link state in bge_poll_locked() */
		if (!(sc->bge_ifp->if_capenable & IFCAP_POLLING))
#endif
		{
		sc->bge_link_evt++;
		BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
		}
	}

	callout_reset(&sc->bge_stat_ch, hz, bge_tick, sc);
}

static void
bge_tick(xsc)
	void *xsc;
{
	struct bge_softc *sc;

	sc = xsc;

	BGE_LOCK(sc);
	bge_tick_locked(sc);
	BGE_UNLOCK(sc);
}

static void
bge_stats_update_regs(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct bge_mac_stats_regs stats;
	u_int32_t *s;
	u_long cnt;			/* current register value */
	int i;

	ifp = sc->bge_ifp;

	s = (u_int32_t *)&stats;
	for (i = 0; i < sizeof(struct bge_mac_stats_regs); i += 4) {
		*s = CSR_READ_4(sc, BGE_RX_STATS + i);
		s++;
	}

	cnt = stats.dot3StatsSingleCollisionFrames +
	    stats.dot3StatsMultipleCollisionFrames +
	    stats.dot3StatsExcessiveCollisions +
	    stats.dot3StatsLateCollisions;
	ifp->if_collisions += cnt >= sc->bge_tx_collisions ?
	    cnt - sc->bge_tx_collisions : cnt;
	sc->bge_tx_collisions = cnt;
}

static void
bge_stats_update(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	bus_size_t stats;
	u_long cnt;			/* current register value */

	ifp = sc->bge_ifp;

	stats = BGE_MEMWIN_START + BGE_STATS_BLOCK;

#define READ_STAT(sc, stats, stat) \
	CSR_READ_4(sc, stats + offsetof(struct bge_stats, stat))

	cnt = READ_STAT(sc, stats,
	    txstats.dot3StatsSingleCollisionFrames.bge_addr_lo);
	cnt += READ_STAT(sc, stats,
	    txstats.dot3StatsMultipleCollisionFrames.bge_addr_lo);
	cnt += READ_STAT(sc, stats,
	    txstats.dot3StatsExcessiveCollisions.bge_addr_lo);
	cnt += READ_STAT(sc, stats,
		txstats.dot3StatsLateCollisions.bge_addr_lo);
	ifp->if_collisions += cnt >= sc->bge_tx_collisions ?
	    cnt - sc->bge_tx_collisions : cnt;
	sc->bge_tx_collisions = cnt;

	cnt = READ_STAT(sc, stats, ifInDiscards.bge_addr_lo);
	ifp->if_ierrors += cnt >= sc->bge_rx_discards ?
	    cnt - sc->bge_rx_discards : cnt;
	sc->bge_rx_discards = cnt;

	cnt = READ_STAT(sc, stats, txstats.ifOutDiscards.bge_addr_lo);
	ifp->if_oerrors += cnt >= sc->bge_tx_discards ?
	    cnt - sc->bge_tx_discards : cnt;
	sc->bge_tx_discards = cnt;

#undef READ_STAT
}

/*
 * Pad outbound frame to ETHER_MIN_NOPAD for an unusual reason.
 * The bge hardware will pad out Tx runts to ETHER_MIN_NOPAD,
 * but when such padded frames employ the bge IP/TCP checksum offload,
 * the hardware checksum assist gives incorrect results (possibly
 * from incorporating its own padding into the UDP/TCP checksum; who knows).
 * If we pad such runts with zeros, the onboard checksum comes out correct.
 */
static __inline int
bge_cksum_pad(struct mbuf *m)
{
	int padlen = ETHER_MIN_NOPAD - m->m_pkthdr.len;
	struct mbuf *last;

	/* If there's only the packet-header and we can pad there, use it. */
	if (m->m_pkthdr.len == m->m_len && M_WRITABLE(m) &&
	    M_TRAILINGSPACE(m) >= padlen) {
		last = m;
	} else {
		/*
		 * Walk packet chain to find last mbuf. We will either
		 * pad there, or append a new mbuf and pad it.
		 */
		for (last = m; last->m_next != NULL; last = last->m_next);
		if (!(M_WRITABLE(last) && M_TRAILINGSPACE(last) >= padlen)) {
			/* Allocate new empty mbuf, pad it. Compact later. */
			struct mbuf *n;

			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == NULL)
				return (ENOBUFS);
			n->m_len = 0;
			last->m_next = n;
			last = n;
		}
	}
	
	/* Now zero the pad area, to avoid the bge cksum-assist bug. */
	memset(mtod(last, caddr_t) + last->m_len, 0, padlen);
	last->m_len += padlen;
	m->m_pkthdr.len += padlen;

	return (0);
}

/*
 * Encapsulate an mbuf chain in the tx ring  by coupling the mbuf data
 * pointers to descriptors.
 */
static int
bge_encap(sc, m_head, txidx)
	struct bge_softc *sc;
	struct mbuf *m_head;
	uint32_t *txidx;
{
	bus_dma_segment_t	segs[BGE_NSEG_NEW];
	bus_dmamap_t		map;
	struct bge_tx_bd	*d = NULL;
	struct m_tag		*mtag;
	uint32_t		idx = *txidx;
	uint16_t		csum_flags = 0;
	int			nsegs, i, error;

	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			csum_flags |= BGE_TXBDFLAG_IP_CSUM;
		if (m_head->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP)) {
			csum_flags |= BGE_TXBDFLAG_TCP_UDP_CSUM;
			if (m_head->m_pkthdr.len < ETHER_MIN_NOPAD &&
			    bge_cksum_pad(m_head) != 0)
				return (ENOBUFS);
		}
		if (m_head->m_flags & M_LASTFRAG)
			csum_flags |= BGE_TXBDFLAG_IP_FRAG_END;
		else if (m_head->m_flags & M_FRAG)
			csum_flags |= BGE_TXBDFLAG_IP_FRAG;
	}

	mtag = VLAN_OUTPUT_TAG(sc->bge_ifp, m_head);

	map = sc->bge_cdata.bge_tx_dmamap[idx];
	error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_mtag, map,
	    m_head, segs, &nsegs, BUS_DMA_NOWAIT);
        if (error) {
		if (error == EFBIG) {
			struct mbuf *m0;

			m0 = m_defrag(m_head, M_DONTWAIT);
			if (m0 == NULL)
				return (ENOBUFS);
			m_head = m0;
			error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_mtag,
			    map, m_head, segs, &nsegs, BUS_DMA_NOWAIT);
		}
		if (error)
			return (error); 
	}

	/*
	 * Sanity check: avoid coming within 16 descriptors
	 * of the end of the ring.
	 */
	if (nsegs > (BGE_TX_RING_CNT - sc->bge_txcnt - 16)) {
		bus_dmamap_unload(sc->bge_cdata.bge_mtag, map);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_mtag, map, BUS_DMASYNC_PREWRITE);

	for (i = 0; ; i++) {
		d = &sc->bge_ldata.bge_tx_ring[idx];
		d->bge_addr.bge_addr_lo = BGE_ADDR_LO(segs[i].ds_addr);
		d->bge_addr.bge_addr_hi = BGE_ADDR_HI(segs[i].ds_addr);
		d->bge_len = segs[i].ds_len;
		d->bge_flags = csum_flags;
		if (i == nsegs - 1)
			break;
		BGE_INC(idx, BGE_TX_RING_CNT);
	}

	/* Mark the last segment as end of packet... */
	d->bge_flags |= BGE_TXBDFLAG_END;
	/* ... and put VLAN tag into first segment.  */
	d = &sc->bge_ldata.bge_tx_ring[*txidx];
	if (mtag != NULL) {
		d->bge_flags |= BGE_TXBDFLAG_VLAN_TAG;
		d->bge_vlan_tag = VLAN_TAG_VALUE(mtag);
	} else
		d->bge_vlan_tag = 0;

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.
	 */
	sc->bge_cdata.bge_tx_dmamap[*txidx] = sc->bge_cdata.bge_tx_dmamap[idx];
	sc->bge_cdata.bge_tx_dmamap[idx] = map;
	sc->bge_cdata.bge_tx_chain[idx] = m_head;
	sc->bge_txcnt += nsegs;

	BGE_INC(idx, BGE_TX_RING_CNT);
	*txidx = idx;

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start_locked(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;
	struct mbuf *m_head = NULL;
	uint32_t prodidx;
	int count = 0;

	sc = ifp->if_softc;

	if (!sc->bge_link || IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		return;

	prodidx = sc->bge_tx_prodidx;

	while(sc->bge_cdata.bge_tx_chain[prodidx] == NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * XXX
		 * The code inside the if() block is never reached since we
		 * must mark CSUM_IP_FRAGS in our if_hwassist to start getting
		 * requests to checksum TCP/UDP in a fragmented packet.
		 *
		 * XXX
		 * safety overkill.  If this is a fragmented packet chain
		 * with delayed TCP/UDP checksums, then only encapsulate
		 * it if we have enough descriptors to handle the entire
		 * chain at once.
		 * (paranoia -- may not actually be needed)
		 */
		if (m_head->m_flags & M_FIRSTFRAG &&
		    m_head->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
			if ((BGE_TX_RING_CNT - sc->bge_txcnt) <
			    m_head->m_pkthdr.csum_data + 16) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
		}

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (bge_encap(sc, m_head, &prodidx)) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		++count;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);
	}

	if (count == 0) {
		/* no packets were dequeued */
		return;
	}

	/* Transmit */
	CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);
	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);

	sc->bge_tx_prodidx = prodidx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;

	sc = ifp->if_softc;
	BGE_LOCK(sc);
	bge_start_locked(ifp);
	BGE_UNLOCK(sc);
}

static void
bge_init_locked(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	u_int16_t *m;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	/* Cancel pending I/O and flush buffers. */
	bge_stop(sc);
	bge_reset(sc);
	bge_chipinit(sc);

	/*
	 * Init the various state machines, ring
	 * control blocks and firmware.
	 */
	if (bge_blockinit(sc)) {
		device_printf(sc->bge_dev, "initialization failure\n");
		return;
	}

	ifp = sc->bge_ifp;

	/* Specify MTU. */
	CSR_WRITE_4(sc, BGE_RX_MTU, ifp->if_mtu +
	    ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN);

	/* Load our MAC address. */
	m = (u_int16_t *)&IFP2ENADDR(sc->bge_ifp)[0];
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_LO, htons(m[0]));
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_HI, (htons(m[1]) << 16) | htons(m[2]));

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC) {
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	} else {
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	}

	/* Program multicast filter. */
	bge_setmulti(sc);

	/* Init RX ring. */
	bge_init_rx_ring_std(sc);

	/*
	 * Workaround for a bug in 5705 ASIC rev A0. Poll the NIC's
	 * memory to insure that the chip has in fact read the first
	 * entry of the ring.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5705_A0) {
		u_int32_t		v, i;
		for (i = 0; i < 10; i++) {
			DELAY(20);
			v = bge_readmem_ind(sc, BGE_STD_RX_RINGS + 8);
			if (v == (MCLBYTES - ETHER_ALIGN))
				break;
		}
		if (i == 10)
			device_printf (sc->bge_dev,
			    "5705 A0 chip failed to load RX ring\n");
	}

	/* Init jumbo RX ring. */
	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		bge_init_rx_ring_jumbo(sc);

	/* Init our RX return ring index */
	sc->bge_rx_saved_considx = 0;

	/* Init TX ring. */
	bge_init_tx_ring(sc);

	/* Turn on transmitter */
	BGE_SETBIT(sc, BGE_TX_MODE, BGE_TXMODE_ENABLE);

	/* Turn on receiver */
	BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);

	/* Tell firmware we're alive. */
	BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

#ifdef DEVICE_POLLING
	/* Disable interrupts if we are polling. */
	if (ifp->if_capenable & IFCAP_POLLING) {
		BGE_SETBIT(sc, BGE_PCI_MISC_CTL,
		    BGE_PCIMISCCTL_MASK_PCI_INTR);
		CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 1);
		CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 1);
		CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 1);
	} else
#endif
	
	/* Enable host interrupts. */
	{
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_CLEAR_INTA);
	BGE_CLRBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 0);
	}
	
	bge_ifmedia_upd(ifp);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->bge_stat_ch, hz, bge_tick, sc);
}

static void
bge_init(xsc)
	void *xsc;
{
	struct bge_softc *sc = xsc;

	BGE_LOCK(sc);
	bge_init_locked(sc);
	BGE_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int
bge_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	ifm = &sc->bge_ifmedia;

	/* If this is a 1000baseX NIC, enable the TBI port. */
	if (sc->bge_tbi) {
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return(EINVAL);
		switch(IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
#ifndef BGE_FAKE_AUTONEG
			/*
			 * The BCM5704 ASIC appears to have a special
			 * mechanism for programming the autoneg
			 * advertisement registers in TBI mode.
			 */
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704) {
				uint32_t sgdig;
				CSR_WRITE_4(sc, BGE_TX_TBI_AUTONEG, 0);
				sgdig = CSR_READ_4(sc, BGE_SGDIG_CFG);
				sgdig |= BGE_SGDIGCFG_AUTO|
				    BGE_SGDIGCFG_PAUSE_CAP|
				    BGE_SGDIGCFG_ASYM_PAUSE;
				CSR_WRITE_4(sc, BGE_SGDIG_CFG,
				    sgdig|BGE_SGDIGCFG_SEND);
				DELAY(5);
				CSR_WRITE_4(sc, BGE_SGDIG_CFG, sgdig);
			}
#endif
			break;
		case IFM_1000_SX:
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
				BGE_CLRBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			} else {
				BGE_SETBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			}
			break;
		default:
			return(EINVAL);
		}
		return(0);
	}

	sc->bge_link_evt++;
	mii = device_get_softc(sc->bge_miibus);
	if (mii->mii_instance) {
		struct mii_softc *miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list))
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current media status.
 */
static void
bge_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct bge_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;

	if (sc->bge_tbi) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;
		if (CSR_READ_4(sc, BGE_MAC_STS) &
		    BGE_MACSTAT_TBI_PCS_SYNCHED)
			ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_1000_SX;
		if (CSR_READ_4(sc, BGE_MAC_MODE) & BGE_MACMODE_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
		else
			ifmr->ifm_active |= IFM_FDX;
		return;
	}

	mii = device_get_softc(sc->bge_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int
bge_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct bge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	int mask, error = 0;
	struct mii_data *mii;

	switch(command) {
	case SIOCSIFMTU:
		/* Disallow jumbo frames on 5705. */
		if (((sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
		      sc->bge_asicrev == BGE_ASICREV_BCM5750) &&
		    ifr->ifr_mtu > ETHERMTU) || ifr->ifr_mtu > BGE_JUMBO_MTU)
			error = EINVAL;
		else {
			ifp->if_mtu = ifr->ifr_mtu;
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			bge_init(sc);
		}
		break;
	case SIOCSIFFLAGS:
		BGE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.  Similarly for ALLMULTI.
			 */
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->bge_if_flags & IFF_PROMISC)) {
				BGE_SETBIT(sc, BGE_RX_MODE,
				    BGE_RXMODE_RX_PROMISC);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->bge_if_flags & IFF_PROMISC) {
				BGE_CLRBIT(sc, BGE_RX_MODE,
				    BGE_RXMODE_RX_PROMISC);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    (ifp->if_flags ^ sc->bge_if_flags) & IFF_ALLMULTI) {
				bge_setmulti(sc);
			} else
				bge_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				bge_stop(sc);
			}
		}
		sc->bge_if_flags = ifp->if_flags;
		BGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			BGE_LOCK(sc);
			bge_setmulti(sc);
			BGE_UNLOCK(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->bge_tbi) {
			error = ifmedia_ioctl(ifp, ifr,
			    &sc->bge_ifmedia, command);
		} else {
			mii = device_get_softc(sc->bge_miibus);
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		}
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(bge_poll, ifp);
				if (error)
					return(error);
				BGE_LOCK(sc);
				BGE_SETBIT(sc, BGE_PCI_MISC_CTL,
				    BGE_PCIMISCCTL_MASK_PCI_INTR);
				CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 1);
				CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 1);
				CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 1);
				ifp->if_capenable |= IFCAP_POLLING;   
				BGE_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				BGE_LOCK(sc);
				CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 0);
				CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 0);
				BGE_CLRBIT(sc, BGE_PCI_MISC_CTL,
				    BGE_PCIMISCCTL_MASK_PCI_INTR);
				CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 0);
				ifp->if_capenable &= ~IFCAP_POLLING;
				BGE_UNLOCK(sc);
			}
		}
#endif
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if (IFCAP_HWCSUM & ifp->if_capenable &&
			    IFCAP_HWCSUM & ifp->if_capabilities)
				ifp->if_hwassist = BGE_CSUM_FEATURES;
			else
				ifp->if_hwassist = 0;
		}
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return(error);
}

static void
bge_watchdog(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;

	sc = ifp->if_softc;

	if_printf(ifp, "watchdog timeout -- resetting\n");

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	bge_init(sc);

	ifp->if_oerrors++;

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
bge_stop(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct ifmedia_entry *ifm;
	struct mii_data *mii = NULL;
	int mtmp, itmp;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	if (!sc->bge_tbi)
		mii = device_get_softc(sc->bge_miibus);

	callout_stop(&sc->bge_stat_ch);

	/*
	 * Disable all of the receiver blocks
	 */
	BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		BGE_CLRBIT(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDBDI_MODE, BGE_RBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE);

	/*
	 * Disable all of the transmit blocks
	 */
	BGE_CLRBIT(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		BGE_CLRBIT(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/*
	 * Shut down all of the memory managers and related
	 * state machines.
	 */
	BGE_CLRBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		BGE_CLRBIT(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		BGE_CLRBIT(sc, BGE_BMAN_MODE, BGE_BMANMODE_ENABLE);
		BGE_CLRBIT(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);
	}

	/* Disable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 1);

	/*
	 * Tell firmware we're shutting down.
	 */
	BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Free the RX lists. */
	bge_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		bge_free_rx_ring_jumbo(sc);

	/* Free TX buffers. */
	bge_free_tx_ring(sc);

	/*
	 * Isolate/power down the PHY, but leave the media selection
	 * unchanged so that things will be put back to normal when
	 * we bring the interface back up.
	 */
	if (!sc->bge_tbi) {
		itmp = ifp->if_flags;
		ifp->if_flags |= IFF_UP;
		/*
		 * If we are called from bge_detach(), mii is already NULL.
		 */
		if (mii != NULL) {
			ifm = mii->mii_media.ifm_cur;
			mtmp = ifm->ifm_media;
			ifm->ifm_media = IFM_ETHER|IFM_NONE;
			mii_mediachg(mii);
			ifm->ifm_media = mtmp;
		}
		ifp->if_flags = itmp;
	}

	sc->bge_tx_saved_considx = BGE_TXCONS_UNSET;

	/*
	 * We can't just call bge_link_upd() cause chip is almost stopped so
	 * bge_link_upd -> bge_tick_locked -> bge_stats_update sequence may
	 * lead to hardware deadlock. So we just clearing MAC's link state
	 * (PHY may still have link UP).
	 */
	if (bootverbose && sc->bge_link)
		if_printf(sc->bge_ifp, "link DOWN\n");
	sc->bge_link = 0;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
bge_shutdown(dev)
	device_t dev;
{
	struct bge_softc *sc;

	sc = device_get_softc(dev);

	BGE_LOCK(sc);
	bge_stop(sc);
	bge_reset(sc);
	BGE_UNLOCK(sc);

	return;
}

static int
bge_suspend(device_t dev)
{
	struct bge_softc *sc;

	sc = device_get_softc(dev);
	BGE_LOCK(sc);
	bge_stop(sc);
	BGE_UNLOCK(sc);

	return (0);
}

static int
bge_resume(device_t dev)
{
	struct bge_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	BGE_LOCK(sc);
	ifp = sc->bge_ifp;
	if (ifp->if_flags & IFF_UP) {
		bge_init_locked(sc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			bge_start_locked(ifp);
	}
	BGE_UNLOCK(sc);

	return (0);
}

static void
bge_link_upd(sc)
	struct bge_softc *sc;
{
	struct mii_data *mii;
	uint32_t link, status;

	BGE_LOCK_ASSERT(sc);

	/* Clear 'pending link event' flag */
	sc->bge_link_evt = 0;

	/*
	 * Process link state changes.
	 * Grrr. The link status word in the status block does
	 * not work correctly on the BCM5700 rev AX and BX chips,
	 * according to all available information. Hence, we have
	 * to enable MII interrupts in order to properly obtain
	 * async link changes. Unfortunately, this also means that
	 * we have to read the MAC status register to detect link
	 * changes, thereby adding an additional register access to
	 * the interrupt handler.
	 *
	 * XXX: perhaps link state detection procedure used for
	 * BGE_CHIPID_BCM5700_B1 can be used for others BCM5700 revisions.
	 */

	if (sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5700_B1) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_MI_INTERRUPT) {
			callout_stop(&sc->bge_stat_ch);
			bge_tick_locked(sc);

			mii = device_get_softc(sc->bge_miibus);
			if (!sc->bge_link &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
				sc->bge_link++;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link UP\n");
			} else if (sc->bge_link &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE)) {
				sc->bge_link = 0;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link DOWN\n");
			}

			/* Clear the interrupt */
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
			bge_miibus_readreg(sc->bge_dev, 1, BRGPHY_MII_ISR);
			bge_miibus_writereg(sc->bge_dev, 1, BRGPHY_MII_IMR,
			    BRGPHY_INTRS);
		}
		return;
	} 

	if (sc->bge_tbi) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_TBI_PCS_SYNCHED) {
			if (!sc->bge_link) {
				sc->bge_link++;
				if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
					BGE_CLRBIT(sc, BGE_MAC_MODE,
					    BGE_MACMODE_TBI_SEND_CFGS);
				CSR_WRITE_4(sc, BGE_MAC_STS, 0xFFFFFFFF);
				if (bootverbose)
					if_printf(sc->bge_ifp, "link UP\n");
				if_link_state_change(sc->bge_ifp, LINK_STATE_UP);
			}
		} else if (sc->bge_link) {
			sc->bge_link = 0;
			if (bootverbose)
				if_printf(sc->bge_ifp, "link DOWN\n");
			if_link_state_change(sc->bge_ifp, LINK_STATE_DOWN);
		}
	/* Discard link events for MII/GMII cards if MI auto-polling disabled */
	} else if (CSR_READ_4(sc, BGE_MI_MODE) & BGE_MIMODE_AUTOPOLL) {
		/* 
		 * Some broken BCM chips have BGE_STATFLAG_LINKSTATE_CHANGED bit
		 * in status word always set. Workaround this bug by reading
		 * PHY link status directly.
		 */
		link = (CSR_READ_4(sc, BGE_MI_STS) & BGE_MISTS_LINK) ? 1 : 0;

		if (link != sc->bge_link ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5700) {
			callout_stop(&sc->bge_stat_ch);
			bge_tick_locked(sc);

			mii = device_get_softc(sc->bge_miibus);
			if (!sc->bge_link &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
				sc->bge_link++;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link UP\n");
			} else if (sc->bge_link &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE)) {
				sc->bge_link = 0;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link DOWN\n");
			}
		}
	}

	/* Clear the attention */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);
}
