/*-
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/dc/if_dc.c 171729 2007-08-05 11:28:19Z marius $");

/*
 * DEC "tulip" clone ethernet driver. Supports the DEC/Intel 21143
 * series chips and several workalikes including the following:
 *
 * Macronix 98713/98715/98725/98727/98732 PMAC (www.macronix.com)
 * Macronix/Lite-On 82c115 PNIC II (www.macronix.com)
 * Lite-On 82c168/82c169 PNIC (www.litecom.com)
 * ASIX Electronics AX88140A (www.asix.com.tw)
 * ASIX Electronics AX88141 (www.asix.com.tw)
 * ADMtek AL981 (www.admtek.com.tw)
 * ADMtek AN985 (www.admtek.com.tw)
 * Netgear FA511 (www.netgear.com) Appears to be rebadged ADMTek AN985
 * Davicom DM9100, DM9102, DM9102A (www.davicom8.com)
 * Accton EN1217 (www.accton.com)
 * Xircom X3201 (www.xircom.com)
 * Abocom FE2500
 * Conexant LANfinity (www.conexant.com)
 * 3Com OfficeConnect 10/100B 3CSOHO100B (www.3com.com)
 *
 * Datasheets for the 21143 are available at developer.intel.com.
 * Datasheets for the clone parts can be found at their respective sites.
 * (Except for the PNIC; see www.freebsd.org/~wpaul/PNIC/pnic.ps.gz.)
 * The PNIC II is essentially a Macronix 98715A chip; the only difference
 * worth noting is that its multicast hash table is only 128 bits wide
 * instead of 512.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */
/*
 * The Intel 21143 is the successor to the DEC 21140. It is basically
 * the same as the 21140 but with a few new features. The 21143 supports
 * three kinds of media attachments:
 *
 * o MII port, for 10Mbps and 100Mbps support and NWAY
 *   autonegotiation provided by an external PHY.
 * o SYM port, for symbol mode 100Mbps support.
 * o 10baseT port.
 * o AUI/BNC port.
 *
 * The 100Mbps SYM port and 10baseT port can be used together in
 * combination with the internal NWAY support to create a 10/100
 * autosensing configuration.
 *
 * Note that not all tulip workalikes are handled in this driver: we only
 * deal with those which are relatively well behaved. The Winbond is
 * handled separately due to its different register offsets and the
 * special handling needed for its various bugs. The PNIC is handled
 * here, but I'm not thrilled about it.
 *
 * All of the workalike chips use some form of MII transceiver support
 * with the exception of the Macronix chips, which also have a SYM port.
 * The ASIX AX88140A is also documented to have a SYM port, but all
 * the cards I've seen use an MII transceiver, probably because the
 * AX88140A doesn't support internal NWAY.
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
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define DC_USEIOSPACE

#include <dev/dc/if_dcreg.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>
#endif

MODULE_DEPEND(dc, pci, 1, 1, 1);
MODULE_DEPEND(dc, ether, 1, 1, 1);
MODULE_DEPEND(dc, miibus, 1, 1, 1);

/*
 * "device miibus" is required in kernel config.  See GENERIC if you get
 * errors here.
 */
#include "miibus_if.h"

/*
 * Various supported device vendors/types and their names.
 */
static struct dc_type dc_devs[] = {
	{ DC_DEVID(DC_VENDORID_DEC, DC_DEVICEID_21143), 0,
		"Intel 21143 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9009), 0,
		"Davicom DM9009 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9100), 0,
		"Davicom DM9100 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9102), DC_REVISION_DM9102A,
		"Davicom DM9102A 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9102), 0,
		"Davicom DM9102 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_AL981), 0,
		"ADMtek AL981 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_AN985), 0,
		"ADMtek AN985 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_ADM9511), 0,
		"ADMtek ADM9511 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_ADM9513), 0,
		"ADMtek ADM9513 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_FA511), 0,
		"Netgear FA511 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ASIX, DC_DEVICEID_AX88140A), DC_REVISION_88141,
		"ASIX AX88141 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ASIX, DC_DEVICEID_AX88140A), 0,
		"ASIX AX88140A 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_98713), DC_REVISION_98713A,
		"Macronix 98713A 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_98713), 0,
		"Macronix 98713 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_CP, DC_DEVICEID_98713_CP), DC_REVISION_98713A,
		"Compex RL100-TX 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_CP, DC_DEVICEID_98713_CP), 0,
		"Compex RL100-TX 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_987x5), DC_REVISION_98725,
		"Macronix 98725 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_987x5), DC_REVISION_98715AEC_C,
		"Macronix 98715AEC-C 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_987x5), 0,
		"Macronix 98715/98715A 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_98727), 0,
		"Macronix 98727/98732 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_LO, DC_DEVICEID_82C115), 0,
		"LC82C115 PNIC II 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_LO, DC_DEVICEID_82C168), DC_REVISION_82C169,
		"82c169 PNIC 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_LO, DC_DEVICEID_82C168), 0,
		"82c168 PNIC 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ACCTON, DC_DEVICEID_EN1217), 0,
		"Accton EN1217 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ACCTON, DC_DEVICEID_EN2242), 0,
		"Accton EN2242 MiniPCI 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_XIRCOM, DC_DEVICEID_X3201), 0,
	  	"Xircom X3201 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_DLINK, DC_DEVICEID_DRP32TXD), 0,
		"Neteasy DRP-32TXD Cardbus 10/100" },
	{ DC_DEVID(DC_VENDORID_ABOCOM, DC_DEVICEID_FE2500), 0,
		"Abocom FE2500 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_ABOCOM, DC_DEVICEID_FE2500MX), 0,
		"Abocom FE2500MX 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_CONEXANT, DC_DEVICEID_RS7112), 0,
		"Conexant LANfinity MiniPCI 10/100BaseTX" },
	{ DC_DEVID(DC_VENDORID_HAWKING, DC_DEVICEID_HAWKING_PN672TX), 0,
		"Hawking CB102 CardBus 10/100" },
	{ DC_DEVID(DC_VENDORID_PLANEX, DC_DEVICEID_FNW3602T), 0,
		"PlaneX FNW-3602-T CardBus 10/100" },
	{ DC_DEVID(DC_VENDORID_3COM, DC_DEVICEID_3CSOHOB), 0,
		"3Com OfficeConnect 10/100B" },
	{ DC_DEVID(DC_VENDORID_MICROSOFT, DC_DEVICEID_MSMN120), 0,
		"Microsoft MN-120 CardBus 10/100" },
	{ DC_DEVID(DC_VENDORID_MICROSOFT, DC_DEVICEID_MSMN130), 0,
		"Microsoft MN-130 10/100" },
	{ DC_DEVID(DC_VENDORID_LINKSYS, DC_DEVICEID_PCMPC200_AB08), 0,
		"Linksys PCMPC200 CardBus 10/100" },
	{ DC_DEVID(DC_VENDORID_LINKSYS, DC_DEVICEID_PCMPC200_AB09), 0,
		"Linksys PCMPC200 CardBus 10/100" },
	{ 0, 0, NULL }
};

static int dc_probe(device_t);
static int dc_attach(device_t);
static int dc_detach(device_t);
static int dc_suspend(device_t);
static int dc_resume(device_t);
static struct dc_type *dc_devtype(device_t);
static int dc_newbuf(struct dc_softc *, int, int);
static int dc_encap(struct dc_softc *, struct mbuf **);
static void dc_pnic_rx_bug_war(struct dc_softc *, int);
static int dc_rx_resync(struct dc_softc *);
static void dc_rxeof(struct dc_softc *);
static void dc_txeof(struct dc_softc *);
static void dc_tick(void *);
static void dc_tx_underrun(struct dc_softc *);
static void dc_intr(void *);
static void dc_start(struct ifnet *);
static void dc_start_locked(struct ifnet *);
static int dc_ioctl(struct ifnet *, u_long, caddr_t);
static void dc_init(void *);
static void dc_init_locked(struct dc_softc *);
static void dc_stop(struct dc_softc *);
static void dc_watchdog(void *);
static void dc_shutdown(device_t);
static int dc_ifmedia_upd(struct ifnet *);
static void dc_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void dc_delay(struct dc_softc *);
static void dc_eeprom_idle(struct dc_softc *);
static void dc_eeprom_putbyte(struct dc_softc *, int);
static void dc_eeprom_getword(struct dc_softc *, int, u_int16_t *);
static void dc_eeprom_getword_pnic(struct dc_softc *, int, u_int16_t *);
static void dc_eeprom_getword_xircom(struct dc_softc *, int, u_int16_t *);
static void dc_eeprom_width(struct dc_softc *);
static void dc_read_eeprom(struct dc_softc *, caddr_t, int, int, int);

static void dc_mii_writebit(struct dc_softc *, int);
static int dc_mii_readbit(struct dc_softc *);
static void dc_mii_sync(struct dc_softc *);
static void dc_mii_send(struct dc_softc *, u_int32_t, int);
static int dc_mii_readreg(struct dc_softc *, struct dc_mii_frame *);
static int dc_mii_writereg(struct dc_softc *, struct dc_mii_frame *);
static int dc_miibus_readreg(device_t, int, int);
static int dc_miibus_writereg(device_t, int, int, int);
static void dc_miibus_statchg(device_t);
static void dc_miibus_mediainit(device_t);

static void dc_setcfg(struct dc_softc *, int);
static uint32_t dc_mchash_le(struct dc_softc *, const uint8_t *);
static uint32_t dc_mchash_be(const uint8_t *);
static void dc_setfilt_21143(struct dc_softc *);
static void dc_setfilt_asix(struct dc_softc *);
static void dc_setfilt_admtek(struct dc_softc *);
static void dc_setfilt_xircom(struct dc_softc *);

static void dc_setfilt(struct dc_softc *);

static void dc_reset(struct dc_softc *);
static int dc_list_rx_init(struct dc_softc *);
static int dc_list_tx_init(struct dc_softc *);

static void dc_read_srom(struct dc_softc *, int);
static void dc_parse_21143_srom(struct dc_softc *);
static void dc_decode_leaf_sia(struct dc_softc *, struct dc_eblock_sia *);
static void dc_decode_leaf_mii(struct dc_softc *, struct dc_eblock_mii *);
static void dc_decode_leaf_sym(struct dc_softc *, struct dc_eblock_sym *);
static void dc_apply_fixup(struct dc_softc *, int);

static void dc_dma_map_txbuf(void *, bus_dma_segment_t *, int, bus_size_t, int);

#ifdef DC_USEIOSPACE
#define DC_RES			SYS_RES_IOPORT
#define DC_RID			DC_PCI_CFBIO
#else
#define DC_RES			SYS_RES_MEMORY
#define DC_RID			DC_PCI_CFBMA
#endif

static device_method_t dc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dc_probe),
	DEVMETHOD(device_attach,	dc_attach),
	DEVMETHOD(device_detach,	dc_detach),
	DEVMETHOD(device_suspend,	dc_suspend),
	DEVMETHOD(device_resume,	dc_resume),
	DEVMETHOD(device_shutdown,	dc_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	dc_miibus_readreg),
	DEVMETHOD(miibus_writereg,	dc_miibus_writereg),
	DEVMETHOD(miibus_statchg,	dc_miibus_statchg),
	DEVMETHOD(miibus_mediainit,	dc_miibus_mediainit),

	{ 0, 0 }
};

static driver_t dc_driver = {
	"dc",
	dc_methods,
	sizeof(struct dc_softc)
};

static devclass_t dc_devclass;

DRIVER_MODULE(dc, cardbus, dc_driver, dc_devclass, 0, 0);
DRIVER_MODULE(dc, pci, dc_driver, dc_devclass, 0, 0);
DRIVER_MODULE(miibus, dc, miibus_driver, miibus_devclass, 0, 0);

#define DC_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | (x))

#define DC_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)	DC_SETBIT(sc, DC_SIO, (x))
#define SIO_CLR(x)	DC_CLRBIT(sc, DC_SIO, (x))

static void
dc_delay(struct dc_softc *sc)
{
	int idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, DC_BUSCTL);
}

static void
dc_eeprom_width(struct dc_softc *sc)
{
	int i;

	/* Force EEPROM to idle state. */
	dc_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	for (i = 3; i--;) {
		if (6 & (1 << i))
			DC_SETBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		else
			DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		dc_delay(sc);
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	for (i = 1; i <= 12; i++) {
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		if (!(CSR_READ_4(sc, DC_SIO) & DC_SIO_EE_DATAOUT)) {
			DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
			dc_delay(sc);
			break;
		}
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	dc_eeprom_idle(sc);

	if (i < 4 || i > 12)
		sc->dc_romwidth = 6;
	else
		sc->dc_romwidth = i;

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	/* Turn off EEPROM access mode. */
	dc_eeprom_idle(sc);
}

static void
dc_eeprom_idle(struct dc_softc *sc)
{
	int i;

	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	for (i = 0; i < 25; i++) {
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);
	CSR_WRITE_4(sc, DC_SIO, 0x00000000);
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
dc_eeprom_putbyte(struct dc_softc *sc, int addr)
{
	int d, i;

	d = DC_EECMD_READ >> 6;
	for (i = 3; i--; ) {
		if (d & (1 << i))
			DC_SETBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		else
			DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		dc_delay(sc);
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	/*
	 * Feed in each bit and strobe the clock.
	 */
	for (i = sc->dc_romwidth; i--;) {
		if (addr & (1 << i)) {
			SIO_SET(DC_SIO_EE_DATAIN);
		} else {
			SIO_CLR(DC_SIO_EE_DATAIN);
		}
		dc_delay(sc);
		SIO_SET(DC_SIO_EE_CLK);
		dc_delay(sc);
		SIO_CLR(DC_SIO_EE_CLK);
		dc_delay(sc);
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 * The PNIC 82c168/82c169 has its own non-standard way to read
 * the EEPROM.
 */
static void
dc_eeprom_getword_pnic(struct dc_softc *sc, int addr, u_int16_t *dest)
{
	int i;
	u_int32_t r;

	CSR_WRITE_4(sc, DC_PN_SIOCTL, DC_PN_EEOPCODE_READ | addr);

	for (i = 0; i < DC_TIMEOUT; i++) {
		DELAY(1);
		r = CSR_READ_4(sc, DC_SIO);
		if (!(r & DC_PN_SIOCTL_BUSY)) {
			*dest = (u_int16_t)(r & 0xFFFF);
			return;
		}
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 * The Xircom X3201 has its own non-standard way to read
 * the EEPROM, too.
 */
static void
dc_eeprom_getword_xircom(struct dc_softc *sc, int addr, u_int16_t *dest)
{

	SIO_SET(DC_SIO_ROMSEL | DC_SIO_ROMCTL_READ);

	addr *= 2;
	CSR_WRITE_4(sc, DC_ROM, addr | 0x160);
	*dest = (u_int16_t)CSR_READ_4(sc, DC_SIO) & 0xff;
	addr += 1;
	CSR_WRITE_4(sc, DC_ROM, addr | 0x160);
	*dest |= ((u_int16_t)CSR_READ_4(sc, DC_SIO) & 0xff) << 8;

	SIO_CLR(DC_SIO_ROMSEL | DC_SIO_ROMCTL_READ);
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
dc_eeprom_getword(struct dc_softc *sc, int addr, u_int16_t *dest)
{
	int i;
	u_int16_t word = 0;

	/* Force EEPROM to idle state. */
	dc_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO,  DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	dc_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(DC_SIO_EE_CLK);
		dc_delay(sc);
		if (CSR_READ_4(sc, DC_SIO) & DC_SIO_EE_DATAOUT)
			word |= i;
		dc_delay(sc);
		SIO_CLR(DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	dc_eeprom_idle(sc);

	*dest = word;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
dc_read_eeprom(struct dc_softc *sc, caddr_t dest, int off, int cnt, int be)
{
	int i;
	u_int16_t word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		if (DC_IS_PNIC(sc))
			dc_eeprom_getword_pnic(sc, off + i, &word);
		else if (DC_IS_XIRCOM(sc))
			dc_eeprom_getword_xircom(sc, off + i, &word);
		else
			dc_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (be)
			*ptr = be16toh(word);
		else
			*ptr = le16toh(word);
	}
}

/*
 * The following two routines are taken from the Macronix 98713
 * Application Notes pp.19-21.
 */
/*
 * Write a bit to the MII bus.
 */
static void
dc_mii_writebit(struct dc_softc *sc, int bit)
{

	if (bit)
		CSR_WRITE_4(sc, DC_SIO,
		    DC_SIO_ROMCTL_WRITE | DC_SIO_MII_DATAOUT);
	else
		CSR_WRITE_4(sc, DC_SIO, DC_SIO_ROMCTL_WRITE);

	DC_SETBIT(sc, DC_SIO, DC_SIO_MII_CLK);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_MII_CLK);
}

/*
 * Read a bit from the MII bus.
 */
static int
dc_mii_readbit(struct dc_softc *sc)
{

	CSR_WRITE_4(sc, DC_SIO, DC_SIO_ROMCTL_READ | DC_SIO_MII_DIR);
	CSR_READ_4(sc, DC_SIO);
	DC_SETBIT(sc, DC_SIO, DC_SIO_MII_CLK);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_MII_CLK);
	if (CSR_READ_4(sc, DC_SIO) & DC_SIO_MII_DATAIN)
		return (1);

	return (0);
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
dc_mii_sync(struct dc_softc *sc)
{
	int i;

	CSR_WRITE_4(sc, DC_SIO, DC_SIO_ROMCTL_WRITE);

	for (i = 0; i < 32; i++)
		dc_mii_writebit(sc, 1);
}

/*
 * Clock a series of bits through the MII.
 */
static void
dc_mii_send(struct dc_softc *sc, u_int32_t bits, int cnt)
{
	int i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1)
		dc_mii_writebit(sc, bits & i);
}

/*
 * Read an PHY register through the MII.
 */
static int
dc_mii_readreg(struct dc_softc *sc, struct dc_mii_frame *frame)
{
	int i, ack;

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = DC_MII_STARTDELIM;
	frame->mii_opcode = DC_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;

	/*
	 * Sync the PHYs.
	 */
	dc_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	dc_mii_send(sc, frame->mii_stdelim, 2);
	dc_mii_send(sc, frame->mii_opcode, 2);
	dc_mii_send(sc, frame->mii_phyaddr, 5);
	dc_mii_send(sc, frame->mii_regaddr, 5);

#ifdef notdef
	/* Idle bit */
	dc_mii_writebit(sc, 1);
	dc_mii_writebit(sc, 0);
#endif

	/* Check for ack. */
	ack = dc_mii_readbit(sc);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for (i = 0; i < 16; i++)
			dc_mii_readbit(sc);
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		if (!ack) {
			if (dc_mii_readbit(sc))
				frame->mii_data |= i;
		}
	}

fail:

	dc_mii_writebit(sc, 0);
	dc_mii_writebit(sc, 0);

	if (ack)
		return (1);
	return (0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
dc_mii_writereg(struct dc_softc *sc, struct dc_mii_frame *frame)
{

	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = DC_MII_STARTDELIM;
	frame->mii_opcode = DC_MII_WRITEOP;
	frame->mii_turnaround = DC_MII_TURNAROUND;

	/*
	 * Sync the PHYs.
	 */
	dc_mii_sync(sc);

	dc_mii_send(sc, frame->mii_stdelim, 2);
	dc_mii_send(sc, frame->mii_opcode, 2);
	dc_mii_send(sc, frame->mii_phyaddr, 5);
	dc_mii_send(sc, frame->mii_regaddr, 5);
	dc_mii_send(sc, frame->mii_turnaround, 2);
	dc_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	dc_mii_writebit(sc, 0);
	dc_mii_writebit(sc, 0);

	return (0);
}

static int
dc_miibus_readreg(device_t dev, int phy, int reg)
{
	struct dc_mii_frame frame;
	struct dc_softc	 *sc;
	int i, rval, phy_reg = 0;

	sc = device_get_softc(dev);
	bzero(&frame, sizeof(frame));

	/*
	 * Note: both the AL981 and AN985 have internal PHYs,
	 * however the AL981 provides direct access to the PHY
	 * registers while the AN985 uses a serial MII interface.
	 * The AN985's MII interface is also buggy in that you
	 * can read from any MII address (0 to 31), but only address 1
	 * behaves normally. To deal with both cases, we pretend
	 * that the PHY is at MII address 1.
	 */
	if (DC_IS_ADMTEK(sc) && phy != DC_ADMTEK_PHYADDR)
		return (0);

	/*
	 * Note: the ukphy probes of the RS7112 report a PHY at
	 * MII address 0 (possibly HomePNA?) and 1 (ethernet)
	 * so we only respond to correct one.
	 */
	if (DC_IS_CONEXANT(sc) && phy != DC_CONEXANT_PHYADDR)
		return (0);

	if (sc->dc_pmode != DC_PMODE_MII) {
		if (phy == (MII_NPHY - 1)) {
			switch (reg) {
			case MII_BMSR:
			/*
			 * Fake something to make the probe
			 * code think there's a PHY here.
			 */
				return (BMSR_MEDIAMASK);
				break;
			case MII_PHYIDR1:
				if (DC_IS_PNIC(sc))
					return (DC_VENDORID_LO);
				return (DC_VENDORID_DEC);
				break;
			case MII_PHYIDR2:
				if (DC_IS_PNIC(sc))
					return (DC_DEVICEID_82C168);
				return (DC_DEVICEID_21143);
				break;
			default:
				return (0);
				break;
			}
		} else
			return (0);
	}

	if (DC_IS_PNIC(sc)) {
		CSR_WRITE_4(sc, DC_PN_MII, DC_PN_MIIOPCODE_READ |
		    (phy << 23) | (reg << 18));
		for (i = 0; i < DC_TIMEOUT; i++) {
			DELAY(1);
			rval = CSR_READ_4(sc, DC_PN_MII);
			if (!(rval & DC_PN_MII_BUSY)) {
				rval &= 0xFFFF;
				return (rval == 0xFFFF ? 0 : rval);
			}
		}
		return (0);
	}

	if (DC_IS_COMET(sc)) {
		switch (reg) {
		case MII_BMCR:
			phy_reg = DC_AL_BMCR;
			break;
		case MII_BMSR:
			phy_reg = DC_AL_BMSR;
			break;
		case MII_PHYIDR1:
			phy_reg = DC_AL_VENID;
			break;
		case MII_PHYIDR2:
			phy_reg = DC_AL_DEVID;
			break;
		case MII_ANAR:
			phy_reg = DC_AL_ANAR;
			break;
		case MII_ANLPAR:
			phy_reg = DC_AL_LPAR;
			break;
		case MII_ANER:
			phy_reg = DC_AL_ANER;
			break;
		default:
			device_printf(dev, "phy_read: bad phy register %x\n",
			    reg);
			return (0);
			break;
		}

		rval = CSR_READ_4(sc, phy_reg) & 0x0000FFFF;

		if (rval == 0xFFFF)
			return (0);
		return (rval);
	}

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	if (sc->dc_type == DC_TYPE_98713) {
		phy_reg = CSR_READ_4(sc, DC_NETCFG);
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg & ~DC_NETCFG_PORTSEL);
	}
	dc_mii_readreg(sc, &frame);
	if (sc->dc_type == DC_TYPE_98713)
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg);

	return (frame.mii_data);
}

static int
dc_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct dc_softc *sc;
	struct dc_mii_frame frame;
	int i, phy_reg = 0;

	sc = device_get_softc(dev);
	bzero(&frame, sizeof(frame));

	if (DC_IS_ADMTEK(sc) && phy != DC_ADMTEK_PHYADDR)
		return (0);

	if (DC_IS_CONEXANT(sc) && phy != DC_CONEXANT_PHYADDR)
		return (0);

	if (DC_IS_PNIC(sc)) {
		CSR_WRITE_4(sc, DC_PN_MII, DC_PN_MIIOPCODE_WRITE |
		    (phy << 23) | (reg << 10) | data);
		for (i = 0; i < DC_TIMEOUT; i++) {
			if (!(CSR_READ_4(sc, DC_PN_MII) & DC_PN_MII_BUSY))
				break;
		}
		return (0);
	}

	if (DC_IS_COMET(sc)) {
		switch (reg) {
		case MII_BMCR:
			phy_reg = DC_AL_BMCR;
			break;
		case MII_BMSR:
			phy_reg = DC_AL_BMSR;
			break;
		case MII_PHYIDR1:
			phy_reg = DC_AL_VENID;
			break;
		case MII_PHYIDR2:
			phy_reg = DC_AL_DEVID;
			break;
		case MII_ANAR:
			phy_reg = DC_AL_ANAR;
			break;
		case MII_ANLPAR:
			phy_reg = DC_AL_LPAR;
			break;
		case MII_ANER:
			phy_reg = DC_AL_ANER;
			break;
		default:
			device_printf(dev, "phy_write: bad phy register %x\n",
			    reg);
			return (0);
			break;
		}

		CSR_WRITE_4(sc, phy_reg, data);
		return (0);
	}

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	if (sc->dc_type == DC_TYPE_98713) {
		phy_reg = CSR_READ_4(sc, DC_NETCFG);
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg & ~DC_NETCFG_PORTSEL);
	}
	dc_mii_writereg(sc, &frame);
	if (sc->dc_type == DC_TYPE_98713)
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg);

	return (0);
}

static void
dc_miibus_statchg(device_t dev)
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = device_get_softc(dev);
	if (DC_IS_ADMTEK(sc))
		return;

	mii = device_get_softc(sc->dc_miibus);
	ifm = &mii->mii_media;
	if (DC_IS_DAVICOM(sc) &&
	    IFM_SUBTYPE(ifm->ifm_media) == IFM_HPNA_1) {
		dc_setcfg(sc, ifm->ifm_media);
		sc->dc_if_media = ifm->ifm_media;
	} else {
		dc_setcfg(sc, mii->mii_media_active);
		sc->dc_if_media = mii->mii_media_active;
	}
}

/*
 * Special support for DM9102A cards with HomePNA PHYs. Note:
 * with the Davicom DM9102A/DM9801 eval board that I have, it seems
 * to be impossible to talk to the management interface of the DM9801
 * PHY (its MDIO pin is not connected to anything). Consequently,
 * the driver has to just 'know' about the additional mode and deal
 * with it itself. *sigh*
 */
static void
dc_miibus_mediainit(device_t dev)
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;
	int rev;

	rev = pci_get_revid(dev);

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->dc_miibus);
	ifm = &mii->mii_media;

	if (DC_IS_DAVICOM(sc) && rev >= DC_REVISION_DM9102A)
		ifmedia_add(ifm, IFM_ETHER | IFM_HPNA_1, 0, NULL);
}

#define DC_BITS_512	9
#define DC_BITS_128	7
#define DC_BITS_64	6

static uint32_t
dc_mchash_le(struct dc_softc *sc, const uint8_t *addr)
{
	uint32_t crc;

	/* Compute CRC for the address value. */
	crc = ether_crc32_le(addr, ETHER_ADDR_LEN);

	/*
	 * The hash table on the PNIC II and the MX98715AEC-C/D/E
	 * chips is only 128 bits wide.
	 */
	if (sc->dc_flags & DC_128BIT_HASH)
		return (crc & ((1 << DC_BITS_128) - 1));

	/* The hash table on the MX98715BEC is only 64 bits wide. */
	if (sc->dc_flags & DC_64BIT_HASH)
		return (crc & ((1 << DC_BITS_64) - 1));

	/* Xircom's hash filtering table is different (read: weird) */
	/* Xircom uses the LEAST significant bits */
	if (DC_IS_XIRCOM(sc)) {
		if ((crc & 0x180) == 0x180)
			return ((crc & 0x0F) + (crc & 0x70) * 3 + (14 << 4));
		else
			return ((crc & 0x1F) + ((crc >> 1) & 0xF0) * 3 +
			    (12 << 4));
	}

	return (crc & ((1 << DC_BITS_512) - 1));
}

/*
 * Calculate CRC of a multicast group address, return the lower 6 bits.
 */
static uint32_t
dc_mchash_be(const uint8_t *addr)
{
	uint32_t crc;

	/* Compute CRC for the address value. */
	crc = ether_crc32_be(addr, ETHER_ADDR_LEN);

	/* Return the filter bit position. */
	return ((crc >> 26) & 0x0000003F);
}

/*
 * 21143-style RX filter setup routine. Filter programming is done by
 * downloading a special setup frame into the TX engine. 21143, Macronix,
 * PNIC, PNIC II and Davicom chips are programmed this way.
 *
 * We always program the chip using 'hash perfect' mode, i.e. one perfect
 * address (our node address) and a 512-bit hash filter for multicast
 * frames. We also sneak the broadcast address into the hash filter since
 * we need that too.
 */
static void
dc_setfilt_21143(struct dc_softc *sc)
{
	uint16_t eaddr[(ETHER_ADDR_LEN+1)/2];
	struct dc_desc *sframe;
	u_int32_t h, *sp;
	struct ifmultiaddr *ifma;
	struct ifnet *ifp;
	int i;

	ifp = sc->dc_ifp;

	i = sc->dc_cdata.dc_tx_prod;
	DC_INC(sc->dc_cdata.dc_tx_prod, DC_TX_LIST_CNT);
	sc->dc_cdata.dc_tx_cnt++;
	sframe = &sc->dc_ldata->dc_tx_list[i];
	sp = sc->dc_cdata.dc_sbuf;
	bzero(sp, DC_SFRAME_LEN);

	sframe->dc_data = htole32(sc->dc_saddr);
	sframe->dc_ctl = htole32(DC_SFRAME_LEN | DC_TXCTL_SETUP |
	    DC_TXCTL_TLINK | DC_FILTER_HASHPERF | DC_TXCTL_FINT);

	sc->dc_cdata.dc_tx_chain[i] = (struct mbuf *)sc->dc_cdata.dc_sbuf;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = dc_mchash_le(sc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sp[h >> 4] |= htole32(1 << (h & 0xF));
	}
	IF_ADDR_UNLOCK(ifp);

	if (ifp->if_flags & IFF_BROADCAST) {
		h = dc_mchash_le(sc, ifp->if_broadcastaddr);
		sp[h >> 4] |= htole32(1 << (h & 0xF));
	}

	/* Set our MAC address. */
	bcopy(IF_LLADDR(sc->dc_ifp), eaddr, ETHER_ADDR_LEN);
	sp[39] = DC_SP_MAC(eaddr[0]);
	sp[40] = DC_SP_MAC(eaddr[1]);
	sp[41] = DC_SP_MAC(eaddr[2]);

	sframe->dc_status = htole32(DC_TXSTAT_OWN);
	CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

	/*
	 * The PNIC takes an exceedingly long time to process its
	 * setup frame; wait 10ms after posting the setup frame
	 * before proceeding, just so it has time to swallow its
	 * medicine.
	 */
	DELAY(10000);

	sc->dc_wdog_timer = 5;
}

static void
dc_setfilt_admtek(struct dc_softc *sc)
{
	uint32_t eaddr[(ETHER_ADDR_LEN+3)/4];
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	int h = 0;
	u_int32_t hashes[2] = { 0, 0 };

	ifp = sc->dc_ifp;

	/* Init our MAC address. */
	bcopy(IF_LLADDR(sc->dc_ifp), eaddr, ETHER_ADDR_LEN);
	CSR_WRITE_4(sc, DC_AL_PAR0, eaddr[0]);
	CSR_WRITE_4(sc, DC_AL_PAR1, eaddr[1]);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	/* First, zot all the existing hash bits. */
	CSR_WRITE_4(sc, DC_AL_MAR0, 0);
	CSR_WRITE_4(sc, DC_AL_MAR1, 0);

	/*
	 * If we're already in promisc or allmulti mode, we
	 * don't have to bother programming the multicast filter.
	 */
	if (ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI))
		return;

	/* Now program new ones. */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (DC_IS_CENTAUR(sc))
			h = dc_mchash_le(sc,
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		else
			h = dc_mchash_be(
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
	}
	IF_ADDR_UNLOCK(ifp);

	CSR_WRITE_4(sc, DC_AL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, DC_AL_MAR1, hashes[1]);
}

static void
dc_setfilt_asix(struct dc_softc *sc)
{
	uint32_t eaddr[(ETHER_ADDR_LEN+3)/4];
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	int h = 0;
	u_int32_t hashes[2] = { 0, 0 };

	ifp = sc->dc_ifp;

	/* Init our MAC address. */
	bcopy(IF_LLADDR(sc->dc_ifp), eaddr, ETHER_ADDR_LEN);
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_PAR0);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, eaddr[0]);
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_PAR1);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, eaddr[1]);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	/*
	 * The ASIX chip has a special bit to enable reception
	 * of broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		DC_SETBIT(sc, DC_NETCFG, DC_AX_NETCFG_RX_BROAD);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_AX_NETCFG_RX_BROAD);

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR0);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, 0);
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR1);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, 0);

	/*
	 * If we're already in promisc or allmulti mode, we
	 * don't have to bother programming the multicast filter.
	 */
	if (ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI))
		return;

	/* now program new ones */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = dc_mchash_be(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
	}
	IF_ADDR_UNLOCK(ifp);

	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR0);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, hashes[0]);
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR1);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, hashes[1]);
}

static void
dc_setfilt_xircom(struct dc_softc *sc)
{
	uint16_t eaddr[(ETHER_ADDR_LEN+1)/2];
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	struct dc_desc *sframe;
	u_int32_t h, *sp;
	int i;

	ifp = sc->dc_ifp;
	DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_TX_ON | DC_NETCFG_RX_ON));

	i = sc->dc_cdata.dc_tx_prod;
	DC_INC(sc->dc_cdata.dc_tx_prod, DC_TX_LIST_CNT);
	sc->dc_cdata.dc_tx_cnt++;
	sframe = &sc->dc_ldata->dc_tx_list[i];
	sp = sc->dc_cdata.dc_sbuf;
	bzero(sp, DC_SFRAME_LEN);

	sframe->dc_data = htole32(sc->dc_saddr);
	sframe->dc_ctl = htole32(DC_SFRAME_LEN | DC_TXCTL_SETUP |
	    DC_TXCTL_TLINK | DC_FILTER_HASHPERF | DC_TXCTL_FINT);

	sc->dc_cdata.dc_tx_chain[i] = (struct mbuf *)sc->dc_cdata.dc_sbuf;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = dc_mchash_le(sc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sp[h >> 4] |= htole32(1 << (h & 0xF));
	}
	IF_ADDR_UNLOCK(ifp);

	if (ifp->if_flags & IFF_BROADCAST) {
		h = dc_mchash_le(sc, ifp->if_broadcastaddr);
		sp[h >> 4] |= htole32(1 << (h & 0xF));
	}

	/* Set our MAC address. */
	bcopy(IF_LLADDR(sc->dc_ifp), eaddr, ETHER_ADDR_LEN);
	sp[0] = DC_SP_MAC(eaddr[0]);
	sp[1] = DC_SP_MAC(eaddr[1]);
	sp[2] = DC_SP_MAC(eaddr[2]);

	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);
	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ON);
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sframe->dc_status = htole32(DC_TXSTAT_OWN);
	CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

	/*
	 * Wait some time...
	 */
	DELAY(1000);

	sc->dc_wdog_timer = 5;
}

static void
dc_setfilt(struct dc_softc *sc)
{

	if (DC_IS_INTEL(sc) || DC_IS_MACRONIX(sc) || DC_IS_PNIC(sc) ||
	    DC_IS_PNICII(sc) || DC_IS_DAVICOM(sc) || DC_IS_CONEXANT(sc))
		dc_setfilt_21143(sc);

	if (DC_IS_ASIX(sc))
		dc_setfilt_asix(sc);

	if (DC_IS_ADMTEK(sc))
		dc_setfilt_admtek(sc);

	if (DC_IS_XIRCOM(sc))
		dc_setfilt_xircom(sc);
}

/*
 * In order to fiddle with the 'full-duplex' and '100Mbps' bits in
 * the netconfig register, we first have to put the transmit and/or
 * receive logic in the idle state.
 */
static void
dc_setcfg(struct dc_softc *sc, int media)
{
	int i, restart = 0, watchdogreg;
	u_int32_t isr;

	if (IFM_SUBTYPE(media) == IFM_NONE)
		return;

	if (CSR_READ_4(sc, DC_NETCFG) & (DC_NETCFG_TX_ON | DC_NETCFG_RX_ON)) {
		restart = 1;
		DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_TX_ON | DC_NETCFG_RX_ON));

		for (i = 0; i < DC_TIMEOUT; i++) {
			isr = CSR_READ_4(sc, DC_ISR);
			if (isr & DC_ISR_TX_IDLE &&
			    ((isr & DC_ISR_RX_STATE) == DC_RXSTATE_STOPPED ||
			    (isr & DC_ISR_RX_STATE) == DC_RXSTATE_WAIT))
				break;
			DELAY(10);
		}

		if (i == DC_TIMEOUT) {
			if (!(isr & DC_ISR_TX_IDLE) && !DC_IS_ASIX(sc))
				device_printf(sc->dc_dev,
				    "%s: failed to force tx to idle state\n",
				    __func__);
			if (!((isr & DC_ISR_RX_STATE) == DC_RXSTATE_STOPPED ||
			    (isr & DC_ISR_RX_STATE) == DC_RXSTATE_WAIT) &&
			    !(DC_IS_CENTAUR(sc) || DC_IS_CONEXANT(sc) ||
			    (DC_IS_DAVICOM(sc) && pci_get_revid(sc->dc_dev) >=
			    DC_REVISION_DM9102A)))
				device_printf(sc->dc_dev,
				    "%s: failed to force rx to idle state\n",
				    __func__);
		}
	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_SPEEDSEL);
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_HEARTBEAT);
		if (sc->dc_pmode == DC_PMODE_MII) {
			if (DC_IS_INTEL(sc)) {
			/* There's a write enable bit here that reads as 1. */
				watchdogreg = CSR_READ_4(sc, DC_WATCHDOG);
				watchdogreg &= ~DC_WDOG_CTLWREN;
				watchdogreg |= DC_WDOG_JABBERDIS;
				CSR_WRITE_4(sc, DC_WATCHDOG, watchdogreg);
			} else {
				DC_SETBIT(sc, DC_WATCHDOG, DC_WDOG_JABBERDIS);
			}
			DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_PCS |
			    DC_NETCFG_PORTSEL | DC_NETCFG_SCRAMBLER));
			if (sc->dc_type == DC_TYPE_98713)
				DC_SETBIT(sc, DC_NETCFG, (DC_NETCFG_PCS |
				    DC_NETCFG_SCRAMBLER));
			if (!DC_IS_DAVICOM(sc))
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_CLRBIT(sc, DC_10BTCTRL, 0xFFFF);
			if (DC_IS_INTEL(sc))
				dc_apply_fixup(sc, IFM_AUTO);
		} else {
			if (DC_IS_PNIC(sc)) {
				DC_PN_GPIO_SETBIT(sc, DC_PN_GPIO_SPEEDSEL);
				DC_PN_GPIO_SETBIT(sc, DC_PN_GPIO_100TX_LOOP);
				DC_SETBIT(sc, DC_PN_NWAY, DC_PN_NWAY_SPEEDSEL);
			}
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PCS);
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_SCRAMBLER);
			if (DC_IS_INTEL(sc))
				dc_apply_fixup(sc,
				    (media & IFM_GMASK) == IFM_FDX ?
				    IFM_100_TX | IFM_FDX : IFM_100_TX);
		}
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_SPEEDSEL);
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_HEARTBEAT);
		if (sc->dc_pmode == DC_PMODE_MII) {
			/* There's a write enable bit here that reads as 1. */
			if (DC_IS_INTEL(sc)) {
				watchdogreg = CSR_READ_4(sc, DC_WATCHDOG);
				watchdogreg &= ~DC_WDOG_CTLWREN;
				watchdogreg |= DC_WDOG_JABBERDIS;
				CSR_WRITE_4(sc, DC_WATCHDOG, watchdogreg);
			} else {
				DC_SETBIT(sc, DC_WATCHDOG, DC_WDOG_JABBERDIS);
			}
			DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_PCS |
			    DC_NETCFG_PORTSEL | DC_NETCFG_SCRAMBLER));
			if (sc->dc_type == DC_TYPE_98713)
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PCS);
			if (!DC_IS_DAVICOM(sc))
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_CLRBIT(sc, DC_10BTCTRL, 0xFFFF);
			if (DC_IS_INTEL(sc))
				dc_apply_fixup(sc, IFM_AUTO);
		} else {
			if (DC_IS_PNIC(sc)) {
				DC_PN_GPIO_CLRBIT(sc, DC_PN_GPIO_SPEEDSEL);
				DC_PN_GPIO_SETBIT(sc, DC_PN_GPIO_100TX_LOOP);
				DC_CLRBIT(sc, DC_PN_NWAY, DC_PN_NWAY_SPEEDSEL);
			}
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_PCS);
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_SCRAMBLER);
			if (DC_IS_INTEL(sc)) {
				DC_CLRBIT(sc, DC_SIARESET, DC_SIA_RESET);
				DC_CLRBIT(sc, DC_10BTCTRL, 0xFFFF);
				if ((media & IFM_GMASK) == IFM_FDX)
					DC_SETBIT(sc, DC_10BTCTRL, 0x7F3D);
				else
					DC_SETBIT(sc, DC_10BTCTRL, 0x7F3F);
				DC_SETBIT(sc, DC_SIARESET, DC_SIA_RESET);
				DC_CLRBIT(sc, DC_10BTCTRL,
				    DC_TCTL_AUTONEGENBL);
				dc_apply_fixup(sc,
				    (media & IFM_GMASK) == IFM_FDX ?
				    IFM_10_T | IFM_FDX : IFM_10_T);
				DELAY(20000);
			}
		}
	}

	/*
	 * If this is a Davicom DM9102A card with a DM9801 HomePNA
	 * PHY and we want HomePNA mode, set the portsel bit to turn
	 * on the external MII port.
	 */
	if (DC_IS_DAVICOM(sc)) {
		if (IFM_SUBTYPE(media) == IFM_HPNA_1) {
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			sc->dc_link = 1;
		} else {
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
		}
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_FULLDUPLEX);
		if (sc->dc_pmode == DC_PMODE_SYM && DC_IS_PNIC(sc))
			DC_SETBIT(sc, DC_PN_NWAY, DC_PN_NWAY_DUPLEX);
	} else {
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_FULLDUPLEX);
		if (sc->dc_pmode == DC_PMODE_SYM && DC_IS_PNIC(sc))
			DC_CLRBIT(sc, DC_PN_NWAY, DC_PN_NWAY_DUPLEX);
	}

	if (restart)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON | DC_NETCFG_RX_ON);
}

static void
dc_reset(struct dc_softc *sc)
{
	int i;

	DC_SETBIT(sc, DC_BUSCTL, DC_BUSCTL_RESET);

	for (i = 0; i < DC_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, DC_BUSCTL) & DC_BUSCTL_RESET))
			break;
	}

	if (DC_IS_ASIX(sc) || DC_IS_ADMTEK(sc) || DC_IS_CONEXANT(sc) ||
	    DC_IS_XIRCOM(sc) || DC_IS_INTEL(sc)) {
		DELAY(10000);
		DC_CLRBIT(sc, DC_BUSCTL, DC_BUSCTL_RESET);
		i = 0;
	}

	if (i == DC_TIMEOUT)
		device_printf(sc->dc_dev, "reset never completed!\n");

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	CSR_WRITE_4(sc, DC_IMR, 0x00000000);
	CSR_WRITE_4(sc, DC_BUSCTL, 0x00000000);
	CSR_WRITE_4(sc, DC_NETCFG, 0x00000000);

	/*
	 * Bring the SIA out of reset. In some cases, it looks
	 * like failing to unreset the SIA soon enough gets it
	 * into a state where it will never come out of reset
	 * until we reset the whole chip again.
	 */
	if (DC_IS_INTEL(sc)) {
		DC_SETBIT(sc, DC_SIARESET, DC_SIA_RESET);
		CSR_WRITE_4(sc, DC_10BTCTRL, 0);
		CSR_WRITE_4(sc, DC_WATCHDOG, 0);
	}
}

static struct dc_type *
dc_devtype(device_t dev)
{
	struct dc_type *t;
	u_int32_t devid;
	u_int8_t rev;

	t = dc_devs;
	devid = pci_get_devid(dev);
	rev = pci_get_revid(dev);

	while (t->dc_name != NULL) {
		if (devid == t->dc_devid && rev >= t->dc_minrev)
			return (t);
		t++;
	}

	return (NULL);
}

/*
 * Probe for a 21143 or clone chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 * We do a little bit of extra work to identify the exact type of
 * chip. The MX98713 and MX98713A have the same PCI vendor/device ID,
 * but different revision IDs. The same is true for 98715/98715A
 * chips and the 98725, as well as the ASIX and ADMtek chips. In some
 * cases, the exact chip revision affects driver behavior.
 */
static int
dc_probe(device_t dev)
{
	struct dc_type *t;

	t = dc_devtype(dev);

	if (t != NULL) {
		device_set_desc(dev, t->dc_name);
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static void
dc_apply_fixup(struct dc_softc *sc, int media)
{
	struct dc_mediainfo *m;
	u_int8_t *p;
	int i;
	u_int32_t reg;

	m = sc->dc_mi;

	while (m != NULL) {
		if (m->dc_media == media)
			break;
		m = m->dc_next;
	}

	if (m == NULL)
		return;

	for (i = 0, p = m->dc_reset_ptr; i < m->dc_reset_len; i++, p += 2) {
		reg = (p[0] | (p[1] << 8)) << 16;
		CSR_WRITE_4(sc, DC_WATCHDOG, reg);
	}

	for (i = 0, p = m->dc_gp_ptr; i < m->dc_gp_len; i++, p += 2) {
		reg = (p[0] | (p[1] << 8)) << 16;
		CSR_WRITE_4(sc, DC_WATCHDOG, reg);
	}
}

static void
dc_decode_leaf_sia(struct dc_softc *sc, struct dc_eblock_sia *l)
{
	struct dc_mediainfo *m;

	m = malloc(sizeof(struct dc_mediainfo), M_DEVBUF, M_NOWAIT | M_ZERO);
	switch (l->dc_sia_code & ~DC_SIA_CODE_EXT) {
	case DC_SIA_CODE_10BT:
		m->dc_media = IFM_10_T;
		break;
	case DC_SIA_CODE_10BT_FDX:
		m->dc_media = IFM_10_T | IFM_FDX;
		break;
	case DC_SIA_CODE_10B2:
		m->dc_media = IFM_10_2;
		break;
	case DC_SIA_CODE_10B5:
		m->dc_media = IFM_10_5;
		break;
	default:
		break;
	}

	/*
	 * We need to ignore CSR13, CSR14, CSR15 for SIA mode.
	 * Things apparently already work for cards that do
	 * supply Media Specific Data.
	 */
	if (l->dc_sia_code & DC_SIA_CODE_EXT) {
		m->dc_gp_len = 2;
		m->dc_gp_ptr =
		(u_int8_t *)&l->dc_un.dc_sia_ext.dc_sia_gpio_ctl;
	} else {
		m->dc_gp_len = 2;
		m->dc_gp_ptr =
		(u_int8_t *)&l->dc_un.dc_sia_noext.dc_sia_gpio_ctl;
	}

	m->dc_next = sc->dc_mi;
	sc->dc_mi = m;

	sc->dc_pmode = DC_PMODE_SIA;
}

static void
dc_decode_leaf_sym(struct dc_softc *sc, struct dc_eblock_sym *l)
{
	struct dc_mediainfo *m;

	m = malloc(sizeof(struct dc_mediainfo), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (l->dc_sym_code == DC_SYM_CODE_100BT)
		m->dc_media = IFM_100_TX;

	if (l->dc_sym_code == DC_SYM_CODE_100BT_FDX)
		m->dc_media = IFM_100_TX | IFM_FDX;

	m->dc_gp_len = 2;
	m->dc_gp_ptr = (u_int8_t *)&l->dc_sym_gpio_ctl;

	m->dc_next = sc->dc_mi;
	sc->dc_mi = m;

	sc->dc_pmode = DC_PMODE_SYM;
}

static void
dc_decode_leaf_mii(struct dc_softc *sc, struct dc_eblock_mii *l)
{
	struct dc_mediainfo *m;
	u_int8_t *p;

	m = malloc(sizeof(struct dc_mediainfo), M_DEVBUF, M_NOWAIT | M_ZERO);
	/* We abuse IFM_AUTO to represent MII. */
	m->dc_media = IFM_AUTO;
	m->dc_gp_len = l->dc_gpr_len;

	p = (u_int8_t *)l;
	p += sizeof(struct dc_eblock_mii);
	m->dc_gp_ptr = p;
	p += 2 * l->dc_gpr_len;
	m->dc_reset_len = *p;
	p++;
	m->dc_reset_ptr = p;

	m->dc_next = sc->dc_mi;
	sc->dc_mi = m;
}

static void
dc_read_srom(struct dc_softc *sc, int bits)
{
	int size;

	size = 2 << bits;
	sc->dc_srom = malloc(size, M_DEVBUF, M_NOWAIT);
	dc_read_eeprom(sc, (caddr_t)sc->dc_srom, 0, (size / 2), 0);
}

static void
dc_parse_21143_srom(struct dc_softc *sc)
{
	struct dc_leaf_hdr *lhdr;
	struct dc_eblock_hdr *hdr;
	int have_mii, i, loff;
	char *ptr;

	have_mii = 0;
	loff = sc->dc_srom[27];
	lhdr = (struct dc_leaf_hdr *)&(sc->dc_srom[loff]);

	ptr = (char *)lhdr;
	ptr += sizeof(struct dc_leaf_hdr) - 1;
	/*
	 * Look if we got a MII media block.
	 */
	for (i = 0; i < lhdr->dc_mcnt; i++) {
		hdr = (struct dc_eblock_hdr *)ptr;
		if (hdr->dc_type == DC_EBLOCK_MII)
		    have_mii++;

		ptr += (hdr->dc_len & 0x7F);
		ptr++;
	}

	/*
	 * Do the same thing again. Only use SIA and SYM media
	 * blocks if no MII media block is available.
	 */
	ptr = (char *)lhdr;
	ptr += sizeof(struct dc_leaf_hdr) - 1;
	for (i = 0; i < lhdr->dc_mcnt; i++) {
		hdr = (struct dc_eblock_hdr *)ptr;
		switch (hdr->dc_type) {
		case DC_EBLOCK_MII:
			dc_decode_leaf_mii(sc, (struct dc_eblock_mii *)hdr);
			break;
		case DC_EBLOCK_SIA:
			if (! have_mii)
				dc_decode_leaf_sia(sc,
				    (struct dc_eblock_sia *)hdr);
			break;
		case DC_EBLOCK_SYM:
			if (! have_mii)
				dc_decode_leaf_sym(sc,
				    (struct dc_eblock_sym *)hdr);
			break;
		default:
			/* Don't care. Yet. */
			break;
		}
		ptr += (hdr->dc_len & 0x7F);
		ptr++;
	}
}

static void
dc_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	u_int32_t *paddr;

	KASSERT(nseg == 1, ("wrong number of segments, should be 1"));
	paddr = arg;
	*paddr = segs->ds_addr;
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
dc_attach(device_t dev)
{
	int tmp = 0;
	uint32_t eaddr[(ETHER_ADDR_LEN+3)/4];
	u_int32_t command;
	struct dc_softc *sc;
	struct ifnet *ifp;
	u_int32_t revision;
	int error = 0, rid, mac_offset;
	int i;
	u_int8_t *mac;

	sc = device_get_softc(dev);
	sc->dc_dev = dev;

	mtx_init(&sc->dc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = DC_RID;
	sc->dc_res = bus_alloc_resource_any(dev, DC_RES, &rid, RF_ACTIVE);

	if (sc->dc_res == NULL) {
		device_printf(dev, "couldn't map ports/memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->dc_btag = rman_get_bustag(sc->dc_res);
	sc->dc_bhandle = rman_get_bushandle(sc->dc_res);

	/* Allocate interrupt. */
	rid = 0;
	sc->dc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->dc_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	/* Need this info to decide on a chip type. */
	sc->dc_info = dc_devtype(dev);
	revision = pci_get_revid(dev);

	/* Get the eeprom width, but PNIC and XIRCOM have diff eeprom */
	if (sc->dc_info->dc_devid !=
	    DC_DEVID(DC_VENDORID_LO, DC_DEVICEID_82C168) &&
	    sc->dc_info->dc_devid !=
	    DC_DEVID(DC_VENDORID_XIRCOM, DC_DEVICEID_X3201))
		dc_eeprom_width(sc);

	switch (sc->dc_info->dc_devid) {
	case DC_DEVID(DC_VENDORID_DEC, DC_DEVICEID_21143):
		sc->dc_type = DC_TYPE_21143;
		sc->dc_flags |= DC_TX_POLL | DC_TX_USE_TX_INTR;
		sc->dc_flags |= DC_REDUCED_MII_POLL;
		/* Save EEPROM contents so we can parse them later. */
		dc_read_srom(sc, sc->dc_romwidth);
		break;
	case DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9009):
	case DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9100):
	case DC_DEVID(DC_VENDORID_DAVICOM, DC_DEVICEID_DM9102):
		sc->dc_type = DC_TYPE_DM9102;
		sc->dc_flags |= DC_TX_COALESCE | DC_TX_INTR_ALWAYS;
		sc->dc_flags |= DC_REDUCED_MII_POLL | DC_TX_STORENFWD;
		sc->dc_flags |= DC_TX_ALIGN;
		sc->dc_pmode = DC_PMODE_MII;

		/* Increase the latency timer value. */
		pci_write_config(dev, PCIR_LATTIMER, 0x80, 1);
		break;
	case DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_AL981):
		sc->dc_type = DC_TYPE_AL981;
		sc->dc_flags |= DC_TX_USE_TX_INTR;
		sc->dc_flags |= DC_TX_ADMTEK_WAR;
		sc->dc_pmode = DC_PMODE_MII;
		dc_read_srom(sc, sc->dc_romwidth);
		break;
	case DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_AN985):
	case DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_ADM9511):
	case DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_ADM9513):
	case DC_DEVID(DC_VENDORID_DLINK, DC_DEVICEID_DRP32TXD):
	case DC_DEVID(DC_VENDORID_ADMTEK, DC_DEVICEID_FA511):
	case DC_DEVID(DC_VENDORID_ABOCOM, DC_DEVICEID_FE2500):
	case DC_DEVID(DC_VENDORID_ABOCOM, DC_DEVICEID_FE2500MX):
	case DC_DEVID(DC_VENDORID_ACCTON, DC_DEVICEID_EN2242):
	case DC_DEVID(DC_VENDORID_HAWKING, DC_DEVICEID_HAWKING_PN672TX):
	case DC_DEVID(DC_VENDORID_PLANEX, DC_DEVICEID_FNW3602T):
	case DC_DEVID(DC_VENDORID_3COM, DC_DEVICEID_3CSOHOB):
	case DC_DEVID(DC_VENDORID_MICROSOFT, DC_DEVICEID_MSMN120):
	case DC_DEVID(DC_VENDORID_MICROSOFT, DC_DEVICEID_MSMN130):
	case DC_DEVID(DC_VENDORID_LINKSYS, DC_DEVICEID_PCMPC200_AB08):
	case DC_DEVID(DC_VENDORID_LINKSYS, DC_DEVICEID_PCMPC200_AB09):
		sc->dc_type = DC_TYPE_AN985;
		sc->dc_flags |= DC_64BIT_HASH;
		sc->dc_flags |= DC_TX_USE_TX_INTR;
		sc->dc_flags |= DC_TX_ADMTEK_WAR;
		sc->dc_pmode = DC_PMODE_MII;
		/* Don't read SROM for - auto-loaded on reset */
		break;
	case DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_98713):
	case DC_DEVID(DC_VENDORID_CP, DC_DEVICEID_98713_CP):
		if (revision < DC_REVISION_98713A) {
			sc->dc_type = DC_TYPE_98713;
		}
		if (revision >= DC_REVISION_98713A) {
			sc->dc_type = DC_TYPE_98713A;
			sc->dc_flags |= DC_21143_NWAY;
		}
		sc->dc_flags |= DC_REDUCED_MII_POLL;
		sc->dc_flags |= DC_TX_POLL | DC_TX_USE_TX_INTR;
		break;
	case DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_987x5):
	case DC_DEVID(DC_VENDORID_ACCTON, DC_DEVICEID_EN1217):
		/*
		 * Macronix MX98715AEC-C/D/E parts have only a
		 * 128-bit hash table. We need to deal with these
		 * in the same manner as the PNIC II so that we
		 * get the right number of bits out of the
		 * CRC routine.
		 */
		if (revision >= DC_REVISION_98715AEC_C &&
		    revision < DC_REVISION_98725)
			sc->dc_flags |= DC_128BIT_HASH;
		sc->dc_type = DC_TYPE_987x5;
		sc->dc_flags |= DC_TX_POLL | DC_TX_USE_TX_INTR;
		sc->dc_flags |= DC_REDUCED_MII_POLL | DC_21143_NWAY;
		break;
	case DC_DEVID(DC_VENDORID_MX, DC_DEVICEID_98727):
		sc->dc_type = DC_TYPE_987x5;
		sc->dc_flags |= DC_TX_POLL | DC_TX_USE_TX_INTR;
		sc->dc_flags |= DC_REDUCED_MII_POLL | DC_21143_NWAY;
		break;
	case DC_DEVID(DC_VENDORID_LO, DC_DEVICEID_82C115):
		sc->dc_type = DC_TYPE_PNICII;
		sc->dc_flags |= DC_TX_POLL | DC_TX_USE_TX_INTR | DC_128BIT_HASH;
		sc->dc_flags |= DC_REDUCED_MII_POLL | DC_21143_NWAY;
		break;
	case DC_DEVID(DC_VENDORID_LO, DC_DEVICEID_82C168):
		sc->dc_type = DC_TYPE_PNIC;
		sc->dc_flags |= DC_TX_STORENFWD | DC_TX_INTR_ALWAYS;
		sc->dc_flags |= DC_PNIC_RX_BUG_WAR;
		sc->dc_pnic_rx_buf = malloc(DC_RXLEN * 5, M_DEVBUF, M_NOWAIT);
		if (revision < DC_REVISION_82C169)
			sc->dc_pmode = DC_PMODE_SYM;
		break;
	case DC_DEVID(DC_VENDORID_ASIX, DC_DEVICEID_AX88140A):
		sc->dc_type = DC_TYPE_ASIX;
		sc->dc_flags |= DC_TX_USE_TX_INTR | DC_TX_INTR_FIRSTFRAG;
		sc->dc_flags |= DC_REDUCED_MII_POLL;
		sc->dc_pmode = DC_PMODE_MII;
		break;
	case DC_DEVID(DC_VENDORID_XIRCOM, DC_DEVICEID_X3201):
		sc->dc_type = DC_TYPE_XIRCOM;
		sc->dc_flags |= DC_TX_INTR_ALWAYS | DC_TX_COALESCE |
				DC_TX_ALIGN;
		/*
		 * We don't actually need to coalesce, but we're doing
		 * it to obtain a double word aligned buffer.
		 * The DC_TX_COALESCE flag is required.
		 */
		sc->dc_pmode = DC_PMODE_MII;
		break;
	case DC_DEVID(DC_VENDORID_CONEXANT, DC_DEVICEID_RS7112):
		sc->dc_type = DC_TYPE_CONEXANT;
		sc->dc_flags |= DC_TX_INTR_ALWAYS;
		sc->dc_flags |= DC_REDUCED_MII_POLL;
		sc->dc_pmode = DC_PMODE_MII;
		dc_read_srom(sc, sc->dc_romwidth);
		break;
	default:
		device_printf(dev, "unknown device: %x\n",
		    sc->dc_info->dc_devid);
		break;
	}

	/* Save the cache line size. */
	if (DC_IS_DAVICOM(sc))
		sc->dc_cachesize = 0;
	else
		sc->dc_cachesize = pci_get_cachelnsz(dev);

	/* Reset the adapter. */
	dc_reset(sc);

	/* Take 21143 out of snooze mode */
	if (DC_IS_INTEL(sc) || DC_IS_XIRCOM(sc)) {
		command = pci_read_config(dev, DC_PCI_CFDD, 4);
		command &= ~(DC_CFDD_SNOOZE_MODE | DC_CFDD_SLEEP_MODE);
		pci_write_config(dev, DC_PCI_CFDD, command, 4);
	}

	/*
	 * Try to learn something about the supported media.
	 * We know that ASIX and ADMtek and Davicom devices
	 * will *always* be using MII media, so that's a no-brainer.
	 * The tricky ones are the Macronix/PNIC II and the
	 * Intel 21143.
	 */
	if (DC_IS_INTEL(sc))
		dc_parse_21143_srom(sc);
	else if (DC_IS_MACRONIX(sc) || DC_IS_PNICII(sc)) {
		if (sc->dc_type == DC_TYPE_98713)
			sc->dc_pmode = DC_PMODE_MII;
		else
			sc->dc_pmode = DC_PMODE_SYM;
	} else if (!sc->dc_pmode)
		sc->dc_pmode = DC_PMODE_MII;

	/*
	 * Get station address from the EEPROM.
	 */
	switch(sc->dc_type) {
	case DC_TYPE_98713:
	case DC_TYPE_98713A:
	case DC_TYPE_987x5:
	case DC_TYPE_PNICII:
		dc_read_eeprom(sc, (caddr_t)&mac_offset,
		    (DC_EE_NODEADDR_OFFSET / 2), 1, 0);
		dc_read_eeprom(sc, (caddr_t)&eaddr, (mac_offset / 2), 3, 0);
		break;
	case DC_TYPE_PNIC:
		dc_read_eeprom(sc, (caddr_t)&eaddr, 0, 3, 1);
		break;
	case DC_TYPE_DM9102:
		dc_read_eeprom(sc, (caddr_t)&eaddr, DC_EE_NODEADDR, 3, 0);
#ifdef __sparc64__
		/*
		 * If this is an onboard dc(4) the station address read from
		 * the EEPROM is all zero and we have to get it from the FCode.
		 */
		if (eaddr[0] == 0 && (eaddr[1] & ~0xffff) == 0)
			OF_getetheraddr(dev, (caddr_t)&eaddr);
#endif
		break;
	case DC_TYPE_21143:
	case DC_TYPE_ASIX:
		dc_read_eeprom(sc, (caddr_t)&eaddr, DC_EE_NODEADDR, 3, 0);
		break;
	case DC_TYPE_AL981:
	case DC_TYPE_AN985:
		eaddr[0] = CSR_READ_4(sc, DC_AL_PAR0);
		eaddr[1] = CSR_READ_4(sc, DC_AL_PAR1);
		break;
	case DC_TYPE_CONEXANT:
		bcopy(sc->dc_srom + DC_CONEXANT_EE_NODEADDR, &eaddr,
		    ETHER_ADDR_LEN);
		break;
	case DC_TYPE_XIRCOM:
		/* The MAC comes from the CIS. */
		mac = pci_get_ether(dev);
		if (!mac) {
			device_printf(dev, "No station address in CIS!\n");
			error = ENXIO;
			goto fail;
		}
		bcopy(mac, eaddr, ETHER_ADDR_LEN);
		break;
	default:
		dc_read_eeprom(sc, (caddr_t)&eaddr, DC_EE_NODEADDR, 3, 0);
		break;
	}

	/* Allocate a busdma tag and DMA safe memory for TX/RX descriptors. */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct dc_list_data), 1, sizeof(struct dc_list_data),
	    0, NULL, NULL, &sc->dc_ltag);
	if (error) {
		device_printf(dev, "failed to allocate busdma tag\n");
		error = ENXIO;
		goto fail;
	}
	error = bus_dmamem_alloc(sc->dc_ltag, (void **)&sc->dc_ldata,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &sc->dc_lmap);
	if (error) {
		device_printf(dev, "failed to allocate DMA safe memory\n");
		error = ENXIO;
		goto fail;
	}
	error = bus_dmamap_load(sc->dc_ltag, sc->dc_lmap, sc->dc_ldata,
	    sizeof(struct dc_list_data), dc_dma_map_addr, &sc->dc_laddr,
	    BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev, "cannot get address of the descriptors\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Allocate a busdma tag and DMA safe memory for the multicast
	 * setup frame.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    DC_SFRAME_LEN + DC_MIN_FRAMELEN, 1, DC_SFRAME_LEN + DC_MIN_FRAMELEN,
	    0, NULL, NULL, &sc->dc_stag);
	if (error) {
		device_printf(dev, "failed to allocate busdma tag\n");
		error = ENXIO;
		goto fail;
	}
	error = bus_dmamem_alloc(sc->dc_stag, (void **)&sc->dc_cdata.dc_sbuf,
	    BUS_DMA_NOWAIT, &sc->dc_smap);
	if (error) {
		device_printf(dev, "failed to allocate DMA safe memory\n");
		error = ENXIO;
		goto fail;
	}
	error = bus_dmamap_load(sc->dc_stag, sc->dc_smap, sc->dc_cdata.dc_sbuf,
	    DC_SFRAME_LEN, dc_dma_map_addr, &sc->dc_saddr, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev, "cannot get address of the descriptors\n");
		error = ENXIO;
		goto fail;
	}

	/* Allocate a busdma tag for mbufs. */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MCLBYTES, DC_TX_LIST_CNT, MCLBYTES,
	    0, NULL, NULL, &sc->dc_mtag);
	if (error) {
		device_printf(dev, "failed to allocate busdma tag\n");
		error = ENXIO;
		goto fail;
	}

	/* Create the TX/RX busdma maps. */
	for (i = 0; i < DC_TX_LIST_CNT; i++) {
		error = bus_dmamap_create(sc->dc_mtag, 0,
		    &sc->dc_cdata.dc_tx_map[i]);
		if (error) {
			device_printf(dev, "failed to init TX ring\n");
			error = ENXIO;
			goto fail;
		}
	}
	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		error = bus_dmamap_create(sc->dc_mtag, 0,
		    &sc->dc_cdata.dc_rx_map[i]);
		if (error) {
			device_printf(dev, "failed to init RX ring\n");
			error = ENXIO;
			goto fail;
		}
	}
	error = bus_dmamap_create(sc->dc_mtag, 0, &sc->dc_sparemap);
	if (error) {
		device_printf(dev, "failed to init RX ring\n");
		error = ENXIO;
		goto fail;
	}

	ifp = sc->dc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = dc_ioctl;
	ifp->if_start = dc_start;
	ifp->if_init = dc_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, DC_TX_LIST_CNT - 1);
	ifp->if_snd.ifq_drv_maxlen = DC_TX_LIST_CNT - 1;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Do MII setup. If this is a 21143, check for a PHY on the
	 * MII bus after applying any necessary fixups to twiddle the
	 * GPIO bits. If we don't end up finding a PHY, restore the
	 * old selection (SIA only or SIA/SYM) and attach the dcphy
	 * driver instead.
	 */
	if (DC_IS_INTEL(sc)) {
		dc_apply_fixup(sc, IFM_AUTO);
		tmp = sc->dc_pmode;
		sc->dc_pmode = DC_PMODE_MII;
	}

	/*
	 * Setup General Purpose port mode and data so the tulip can talk
	 * to the MII.  This needs to be done before mii_phy_probe so that
	 * we can actually see them.
	 */
	if (DC_IS_XIRCOM(sc)) {
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_WRITE_EN | DC_SIAGP_INT1_EN |
		    DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_INT1_EN |
		    DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
	}

	error = mii_phy_probe(dev, &sc->dc_miibus,
	    dc_ifmedia_upd, dc_ifmedia_sts);

	if (error && DC_IS_INTEL(sc)) {
		sc->dc_pmode = tmp;
		if (sc->dc_pmode != DC_PMODE_SIA)
			sc->dc_pmode = DC_PMODE_SYM;
		sc->dc_flags |= DC_21143_NWAY;
		mii_phy_probe(dev, &sc->dc_miibus,
		    dc_ifmedia_upd, dc_ifmedia_sts);
		/*
		 * For non-MII cards, we need to have the 21143
		 * drive the LEDs. Except there are some systems
		 * like the NEC VersaPro NoteBook PC which have no
		 * LEDs, and twiddling these bits has adverse effects
		 * on them. (I.e. you suddenly can't get a link.)
		 */
		if (!(pci_get_subvendor(dev) == 0x1033 &&
		    pci_get_subdevice(dev) == 0x8028))
			sc->dc_flags |= DC_TULIP_LEDS;
		error = 0;
	}

	if (error) {
		device_printf(dev, "MII without any PHY!\n");
		goto fail;
	}

	if (DC_IS_ADMTEK(sc)) {
		/*
		 * Set automatic TX underrun recovery for the ADMtek chips
		 */
		DC_SETBIT(sc, DC_AL_CR, DC_AL_CR_ATUR);
	}

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	callout_init_mtx(&sc->dc_stat_ch, &sc->dc_mtx, 0);
	callout_init_mtx(&sc->dc_wdog_ch, &sc->dc_mtx, 0);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, (caddr_t)eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->dc_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dc_intr, sc, &sc->dc_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		dc_detach(dev);
	return (error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
dc_detach(device_t dev)
{
	struct dc_softc *sc;
	struct ifnet *ifp;
	struct dc_mediainfo *m;
	int i;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->dc_mtx), ("dc mutex not initialized"));

	ifp = sc->dc_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		DC_LOCK(sc);
		dc_stop(sc);
		DC_UNLOCK(sc);
		callout_drain(&sc->dc_stat_ch);
		callout_drain(&sc->dc_wdog_ch);
		ether_ifdetach(ifp);
	}
	if (sc->dc_miibus)
		device_delete_child(dev, sc->dc_miibus);
	bus_generic_detach(dev);

	if (sc->dc_intrhand)
		bus_teardown_intr(dev, sc->dc_irq, sc->dc_intrhand);
	if (sc->dc_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->dc_irq);
	if (sc->dc_res)
		bus_release_resource(dev, DC_RES, DC_RID, sc->dc_res);

	if (ifp)
		if_free(ifp);

	if (sc->dc_cdata.dc_sbuf != NULL)
		bus_dmamem_free(sc->dc_stag, sc->dc_cdata.dc_sbuf, sc->dc_smap);
	if (sc->dc_ldata != NULL)
		bus_dmamem_free(sc->dc_ltag, sc->dc_ldata, sc->dc_lmap);
	if (sc->dc_mtag) {
		for (i = 0; i < DC_TX_LIST_CNT; i++)
			if (sc->dc_cdata.dc_tx_map[i] != NULL)
				bus_dmamap_destroy(sc->dc_mtag,
				    sc->dc_cdata.dc_tx_map[i]);
		for (i = 0; i < DC_RX_LIST_CNT; i++)
			if (sc->dc_cdata.dc_rx_map[i] != NULL)
				bus_dmamap_destroy(sc->dc_mtag,
				    sc->dc_cdata.dc_rx_map[i]);
		bus_dmamap_destroy(sc->dc_mtag, sc->dc_sparemap);
	}
	if (sc->dc_stag)
		bus_dma_tag_destroy(sc->dc_stag);
	if (sc->dc_mtag)
		bus_dma_tag_destroy(sc->dc_mtag);
	if (sc->dc_ltag)
		bus_dma_tag_destroy(sc->dc_ltag);

	free(sc->dc_pnic_rx_buf, M_DEVBUF);

	while (sc->dc_mi != NULL) {
		m = sc->dc_mi->dc_next;
		free(sc->dc_mi, M_DEVBUF);
		sc->dc_mi = m;
	}
	free(sc->dc_srom, M_DEVBUF);

	mtx_destroy(&sc->dc_mtx);

	return (0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
dc_list_tx_init(struct dc_softc *sc)
{
	struct dc_chain_data *cd;
	struct dc_list_data *ld;
	int i, nexti;

	cd = &sc->dc_cdata;
	ld = sc->dc_ldata;
	for (i = 0; i < DC_TX_LIST_CNT; i++) {
		if (i == DC_TX_LIST_CNT - 1)
			nexti = 0;
		else
			nexti = i + 1;
		ld->dc_tx_list[i].dc_next = htole32(DC_TXDESC(sc, nexti));
		cd->dc_tx_chain[i] = NULL;
		ld->dc_tx_list[i].dc_data = 0;
		ld->dc_tx_list[i].dc_ctl = 0;
	}

	cd->dc_tx_prod = cd->dc_tx_cons = cd->dc_tx_cnt = 0;
	bus_dmamap_sync(sc->dc_ltag, sc->dc_lmap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	return (0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
dc_list_rx_init(struct dc_softc *sc)
{
	struct dc_chain_data *cd;
	struct dc_list_data *ld;
	int i, nexti;

	cd = &sc->dc_cdata;
	ld = sc->dc_ldata;

	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		if (dc_newbuf(sc, i, 1) != 0)
			return (ENOBUFS);
		if (i == DC_RX_LIST_CNT - 1)
			nexti = 0;
		else
			nexti = i + 1;
		ld->dc_rx_list[i].dc_next = htole32(DC_RXDESC(sc, nexti));
	}

	cd->dc_rx_prod = 0;
	bus_dmamap_sync(sc->dc_ltag, sc->dc_lmap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
dc_newbuf(struct dc_softc *sc, int i, int alloc)
{
	struct mbuf *m_new;
	bus_dmamap_t tmp;
	bus_dma_segment_t segs[1];
	int error, nseg;

	if (alloc) {
		m_new = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m_new == NULL)
			return (ENOBUFS);
	} else {
		m_new = sc->dc_cdata.dc_rx_chain[i];
		m_new->m_data = m_new->m_ext.ext_buf;
	}
	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	m_adj(m_new, sizeof(u_int64_t));

	/*
	 * If this is a PNIC chip, zero the buffer. This is part
	 * of the workaround for the receive bug in the 82c168 and
	 * 82c169 chips.
	 */
	if (sc->dc_flags & DC_PNIC_RX_BUG_WAR)
		bzero(mtod(m_new, char *), m_new->m_len);

	/* No need to remap the mbuf if we're reusing it. */
	if (alloc) {
		error = bus_dmamap_load_mbuf_sg(sc->dc_mtag, sc->dc_sparemap,
		    m_new, segs, &nseg, 0);
		KASSERT(nseg == 1, ("wrong number of segments, should be 1"));
		if (error) {
			m_freem(m_new);
			return (error);
		}
		sc->dc_ldata->dc_rx_list[i].dc_data = htole32(segs->ds_addr);
		bus_dmamap_unload(sc->dc_mtag, sc->dc_cdata.dc_rx_map[i]);
		tmp = sc->dc_cdata.dc_rx_map[i];
		sc->dc_cdata.dc_rx_map[i] = sc->dc_sparemap;
		sc->dc_sparemap = tmp;
		sc->dc_cdata.dc_rx_chain[i] = m_new;
	}

	sc->dc_ldata->dc_rx_list[i].dc_ctl = htole32(DC_RXCTL_RLINK | DC_RXLEN);
	sc->dc_ldata->dc_rx_list[i].dc_status = htole32(DC_RXSTAT_OWN);
	bus_dmamap_sync(sc->dc_mtag, sc->dc_cdata.dc_rx_map[i],
	    BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->dc_ltag, sc->dc_lmap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	return (0);
}

/*
 * Grrrrr.
 * The PNIC chip has a terrible bug in it that manifests itself during
 * periods of heavy activity. The exact mode of failure if difficult to
 * pinpoint: sometimes it only happens in promiscuous mode, sometimes it
 * will happen on slow machines. The bug is that sometimes instead of
 * uploading one complete frame during reception, it uploads what looks
 * like the entire contents of its FIFO memory. The frame we want is at
 * the end of the whole mess, but we never know exactly how much data has
 * been uploaded, so salvaging the frame is hard.
 *
 * There is only one way to do it reliably, and it's disgusting.
 * Here's what we know:
 *
 * - We know there will always be somewhere between one and three extra
 *   descriptors uploaded.
 *
 * - We know the desired received frame will always be at the end of the
 *   total data upload.
 *
 * - We know the size of the desired received frame because it will be
 *   provided in the length field of the status word in the last descriptor.
 *
 * Here's what we do:
 *
 * - When we allocate buffers for the receive ring, we bzero() them.
 *   This means that we know that the buffer contents should be all
 *   zeros, except for data uploaded by the chip.
 *
 * - We also force the PNIC chip to upload frames that include the
 *   ethernet CRC at the end.
 *
 * - We gather all of the bogus frame data into a single buffer.
 *
 * - We then position a pointer at the end of this buffer and scan
 *   backwards until we encounter the first non-zero byte of data.
 *   This is the end of the received frame. We know we will encounter
 *   some data at the end of the frame because the CRC will always be
 *   there, so even if the sender transmits a packet of all zeros,
 *   we won't be fooled.
 *
 * - We know the size of the actual received frame, so we subtract
 *   that value from the current pointer location. This brings us
 *   to the start of the actual received packet.
 *
 * - We copy this into an mbuf and pass it on, along with the actual
 *   frame length.
 *
 * The performance hit is tremendous, but it beats dropping frames all
 * the time.
 */

#define DC_WHOLEFRAME	(DC_RXSTAT_FIRSTFRAG | DC_RXSTAT_LASTFRAG)
static void
dc_pnic_rx_bug_war(struct dc_softc *sc, int idx)
{
	struct dc_desc *cur_rx;
	struct dc_desc *c = NULL;
	struct mbuf *m = NULL;
	unsigned char *ptr;
	int i, total_len;
	u_int32_t rxstat = 0;

	i = sc->dc_pnic_rx_bug_save;
	cur_rx = &sc->dc_ldata->dc_rx_list[idx];
	ptr = sc->dc_pnic_rx_buf;
	bzero(ptr, DC_RXLEN * 5);

	/* Copy all the bytes from the bogus buffers. */
	while (1) {
		c = &sc->dc_ldata->dc_rx_list[i];
		rxstat = le32toh(c->dc_status);
		m = sc->dc_cdata.dc_rx_chain[i];
		bcopy(mtod(m, char *), ptr, DC_RXLEN);
		ptr += DC_RXLEN;
		/* If this is the last buffer, break out. */
		if (i == idx || rxstat & DC_RXSTAT_LASTFRAG)
			break;
		dc_newbuf(sc, i, 0);
		DC_INC(i, DC_RX_LIST_CNT);
	}

	/* Find the length of the actual receive frame. */
	total_len = DC_RXBYTES(rxstat);

	/* Scan backwards until we hit a non-zero byte. */
	while (*ptr == 0x00)
		ptr--;

	/* Round off. */
	if ((uintptr_t)(ptr) & 0x3)
		ptr -= 1;

	/* Now find the start of the frame. */
	ptr -= total_len;
	if (ptr < sc->dc_pnic_rx_buf)
		ptr = sc->dc_pnic_rx_buf;

	/*
	 * Now copy the salvaged frame to the last mbuf and fake up
	 * the status word to make it look like a successful
	 * frame reception.
	 */
	dc_newbuf(sc, i, 0);
	bcopy(ptr, mtod(m, char *), total_len);
	cur_rx->dc_status = htole32(rxstat | DC_RXSTAT_FIRSTFRAG);
}

/*
 * This routine searches the RX ring for dirty descriptors in the
 * event that the rxeof routine falls out of sync with the chip's
 * current descriptor pointer. This may happen sometimes as a result
 * of a "no RX buffer available" condition that happens when the chip
 * consumes all of the RX buffers before the driver has a chance to
 * process the RX ring. This routine may need to be called more than
 * once to bring the driver back in sync with the chip, however we
 * should still be getting RX DONE interrupts to drive the search
 * for new packets in the RX ring, so we should catch up eventually.
 */
static int
dc_rx_resync(struct dc_softc *sc)
{
	struct dc_desc *cur_rx;
	int i, pos;

	pos = sc->dc_cdata.dc_rx_prod;

	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		cur_rx = &sc->dc_ldata->dc_rx_list[pos];
		if (!(le32toh(cur_rx->dc_status) & DC_RXSTAT_OWN))
			break;
		DC_INC(pos, DC_RX_LIST_CNT);
	}

	/* If the ring really is empty, then just return. */
	if (i == DC_RX_LIST_CNT)
		return (0);

	/* We've fallen behing the chip: catch it. */
	sc->dc_cdata.dc_rx_prod = pos;

	return (EAGAIN);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
dc_rxeof(struct dc_softc *sc)
{
	struct mbuf *m, *m0;
	struct ifnet *ifp;
	struct dc_desc *cur_rx;
	int i, total_len = 0;
	u_int32_t rxstat;

	DC_LOCK_ASSERT(sc);

	ifp = sc->dc_ifp;
	i = sc->dc_cdata.dc_rx_prod;

	bus_dmamap_sync(sc->dc_ltag, sc->dc_lmap, BUS_DMASYNC_POSTREAD);
	while (!(le32toh(sc->dc_ldata->dc_rx_list[i].dc_status) &
	    DC_RXSTAT_OWN)) {
#ifdef DEVICE_POLLING
		if (ifp->if_capenable & IFCAP_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif
		cur_rx = &sc->dc_ldata->dc_rx_list[i];
		rxstat = le32toh(cur_rx->dc_status);
		m = sc->dc_cdata.dc_rx_chain[i];
		bus_dmamap_sync(sc->dc_mtag, sc->dc_cdata.dc_rx_map[i],
		    BUS_DMASYNC_POSTREAD);
		total_len = DC_RXBYTES(rxstat);

		if (sc->dc_flags & DC_PNIC_RX_BUG_WAR) {
			if ((rxstat & DC_WHOLEFRAME) != DC_WHOLEFRAME) {
				if (rxstat & DC_RXSTAT_FIRSTFRAG)
					sc->dc_pnic_rx_bug_save = i;
				if ((rxstat & DC_RXSTAT_LASTFRAG) == 0) {
					DC_INC(i, DC_RX_LIST_CNT);
					continue;
				}
				dc_pnic_rx_bug_war(sc, i);
				rxstat = le32toh(cur_rx->dc_status);
				total_len = DC_RXBYTES(rxstat);
			}
		}

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
		 * comes up in the ring.  However, don't report long
		 * frames as errors since they could be vlans.
		 */
		if ((rxstat & DC_RXSTAT_RXERR)) {
			if (!(rxstat & DC_RXSTAT_GIANT) ||
			    (rxstat & (DC_RXSTAT_CRCERR | DC_RXSTAT_DRIBBLE |
				       DC_RXSTAT_MIIERE | DC_RXSTAT_COLLSEEN |
				       DC_RXSTAT_RUNT   | DC_RXSTAT_DE))) {
				ifp->if_ierrors++;
				if (rxstat & DC_RXSTAT_COLLSEEN)
					ifp->if_collisions++;
				dc_newbuf(sc, i, 0);
				if (rxstat & DC_RXSTAT_CRCERR) {
					DC_INC(i, DC_RX_LIST_CNT);
					continue;
				} else {
					dc_init_locked(sc);
					return;
				}
			}
		}

		/* No errors; receive the packet. */
		total_len -= ETHER_CRC_LEN;
#ifdef __NO_STRICT_ALIGNMENT
		/*
		 * On architectures without alignment problems we try to
		 * allocate a new buffer for the receive ring, and pass up
		 * the one where the packet is already, saving the expensive
		 * copy done in m_devget().
		 * If we are on an architecture with alignment problems, or
		 * if the allocation fails, then use m_devget and leave the
		 * existing buffer in the receive ring.
		 */
		if (dc_newbuf(sc, i, 1) == 0) {
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
			DC_INC(i, DC_RX_LIST_CNT);
		} else
#endif
		{
			m0 = m_devget(mtod(m, char *), total_len,
				ETHER_ALIGN, ifp, NULL);
			dc_newbuf(sc, i, 0);
			DC_INC(i, DC_RX_LIST_CNT);
			if (m0 == NULL) {
				ifp->if_ierrors++;
				continue;
			}
			m = m0;
		}

		ifp->if_ipackets++;
		DC_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		DC_LOCK(sc);
	}

	sc->dc_cdata.dc_rx_prod = i;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
dc_txeof(struct dc_softc *sc)
{
	struct dc_desc *cur_tx = NULL;
	struct ifnet *ifp;
	int idx;
	u_int32_t ctl, txstat;

	ifp = sc->dc_ifp;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	bus_dmamap_sync(sc->dc_ltag, sc->dc_lmap, BUS_DMASYNC_POSTREAD);
	idx = sc->dc_cdata.dc_tx_cons;
	while (idx != sc->dc_cdata.dc_tx_prod) {

		cur_tx = &sc->dc_ldata->dc_tx_list[idx];
		txstat = le32toh(cur_tx->dc_status);
		ctl = le32toh(cur_tx->dc_ctl);

		if (txstat & DC_TXSTAT_OWN)
			break;

		if (!(ctl & DC_TXCTL_LASTFRAG) || ctl & DC_TXCTL_SETUP) {
			if (ctl & DC_TXCTL_SETUP) {
				/*
				 * Yes, the PNIC is so brain damaged
				 * that it will sometimes generate a TX
				 * underrun error while DMAing the RX
				 * filter setup frame. If we detect this,
				 * we have to send the setup frame again,
				 * or else the filter won't be programmed
				 * correctly.
				 */
				if (DC_IS_PNIC(sc)) {
					if (txstat & DC_TXSTAT_ERRSUM)
						dc_setfilt(sc);
				}
				sc->dc_cdata.dc_tx_chain[idx] = NULL;
			}
			sc->dc_cdata.dc_tx_cnt--;
			DC_INC(idx, DC_TX_LIST_CNT);
			continue;
		}

		if (DC_IS_XIRCOM(sc) || DC_IS_CONEXANT(sc)) {
			/*
			 * XXX: Why does my Xircom taunt me so?
			 * For some reason it likes setting the CARRLOST flag
			 * even when the carrier is there. wtf?!?
			 * Who knows, but Conexant chips have the
			 * same problem. Maybe they took lessons
			 * from Xircom.
			 */
			if (/*sc->dc_type == DC_TYPE_21143 &&*/
			    sc->dc_pmode == DC_PMODE_MII &&
			    ((txstat & 0xFFFF) & ~(DC_TXSTAT_ERRSUM |
			    DC_TXSTAT_NOCARRIER)))
				txstat &= ~DC_TXSTAT_ERRSUM;
		} else {
			if (/*sc->dc_type == DC_TYPE_21143 &&*/
			    sc->dc_pmode == DC_PMODE_MII &&
			    ((txstat & 0xFFFF) & ~(DC_TXSTAT_ERRSUM |
			    DC_TXSTAT_NOCARRIER | DC_TXSTAT_CARRLOST)))
				txstat &= ~DC_TXSTAT_ERRSUM;
		}

		if (txstat & DC_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & DC_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & DC_TXSTAT_LATECOLL)
				ifp->if_collisions++;
			if (!(txstat & DC_TXSTAT_UNDERRUN)) {
				dc_init_locked(sc);
				return;
			}
		}

		ifp->if_collisions += (txstat & DC_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		if (sc->dc_cdata.dc_tx_chain[idx] != NULL) {
			bus_dmamap_sync(sc->dc_mtag,
			    sc->dc_cdata.dc_tx_map[idx],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dc_mtag,
			    sc->dc_cdata.dc_tx_map[idx]);
			m_freem(sc->dc_cdata.dc_tx_chain[idx]);
			sc->dc_cdata.dc_tx_chain[idx] = NULL;
		}

		sc->dc_cdata.dc_tx_cnt--;
		DC_INC(idx, DC_TX_LIST_CNT);
	}
	sc->dc_cdata.dc_tx_cons = idx;

	if (DC_TX_LIST_CNT - sc->dc_cdata.dc_tx_cnt > DC_TX_LIST_RSVD)
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (sc->dc_cdata.dc_tx_cnt == 0)
		sc->dc_wdog_timer = 0;
}

static void
dc_tick(void *xsc)
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;
	u_int32_t r;

	sc = xsc;
	DC_LOCK_ASSERT(sc);
	ifp = sc->dc_ifp;
	mii = device_get_softc(sc->dc_miibus);

	if (sc->dc_flags & DC_REDUCED_MII_POLL) {
		if (sc->dc_flags & DC_21143_NWAY) {
			r = CSR_READ_4(sc, DC_10BTSTAT);
			if (IFM_SUBTYPE(mii->mii_media_active) ==
			    IFM_100_TX && (r & DC_TSTAT_LS100)) {
				sc->dc_link = 0;
				mii_mediachg(mii);
			}
			if (IFM_SUBTYPE(mii->mii_media_active) ==
			    IFM_10_T && (r & DC_TSTAT_LS10)) {
				sc->dc_link = 0;
				mii_mediachg(mii);
			}
			if (sc->dc_link == 0)
				mii_tick(mii);
		} else {
			r = CSR_READ_4(sc, DC_ISR);
			if ((r & DC_ISR_RX_STATE) == DC_RXSTATE_WAIT &&
			    sc->dc_cdata.dc_tx_cnt == 0) {
				mii_tick(mii);
				if (!(mii->mii_media_status & IFM_ACTIVE))
					sc->dc_link = 0;
			}
		}
	} else
		mii_tick(mii);

	/*
	 * When the init routine completes, we expect to be able to send
	 * packets right away, and in fact the network code will send a
	 * gratuitous ARP the moment the init routine marks the interface
	 * as running. However, even though the MAC may have been initialized,
	 * there may be a delay of a few seconds before the PHY completes
	 * autonegotiation and the link is brought up. Any transmissions
	 * made during that delay will be lost. Dealing with this is tricky:
	 * we can't just pause in the init routine while waiting for the
	 * PHY to come ready since that would bring the whole system to
	 * a screeching halt for several seconds.
	 *
	 * What we do here is prevent the TX start routine from sending
	 * any packets until a link has been established. After the
	 * interface has been initialized, the tick routine will poll
	 * the state of the PHY until the IFM_ACTIVE flag is set. Until
	 * that time, packets will stay in the send queue, and once the
	 * link comes up, they will be flushed out to the wire.
	 */
	if (!sc->dc_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->dc_link++;
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			dc_start_locked(ifp);
	}

	if (sc->dc_flags & DC_21143_NWAY && !sc->dc_link)
		callout_reset(&sc->dc_stat_ch, hz/10, dc_tick, sc);
	else
		callout_reset(&sc->dc_stat_ch, hz, dc_tick, sc);
}

/*
 * A transmit underrun has occurred.  Back off the transmit threshold,
 * or switch to store and forward mode if we have to.
 */
static void
dc_tx_underrun(struct dc_softc *sc)
{
	u_int32_t isr;
	int i;

	if (DC_IS_DAVICOM(sc))
		dc_init_locked(sc);

	if (DC_IS_INTEL(sc)) {
		/*
		 * The real 21143 requires that the transmitter be idle
		 * in order to change the transmit threshold or store
		 * and forward state.
		 */
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);

		for (i = 0; i < DC_TIMEOUT; i++) {
			isr = CSR_READ_4(sc, DC_ISR);
			if (isr & DC_ISR_TX_IDLE)
				break;
			DELAY(10);
		}
		if (i == DC_TIMEOUT) {
			device_printf(sc->dc_dev,
			    "%s: failed to force tx to idle state\n",
			    __func__);
			dc_init_locked(sc);
		}
	}

	device_printf(sc->dc_dev, "TX underrun -- ");
	sc->dc_txthresh += DC_TXTHRESH_INC;
	if (sc->dc_txthresh > DC_TXTHRESH_MAX) {
		printf("using store and forward mode\n");
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
	} else {
		printf("increasing TX threshold\n");
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_TX_THRESH);
		DC_SETBIT(sc, DC_NETCFG, sc->dc_txthresh);
	}

	if (DC_IS_INTEL(sc))
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);
}

#ifdef DEVICE_POLLING
static poll_handler_t dc_poll;

static void
dc_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct dc_softc *sc = ifp->if_softc;

	DC_LOCK(sc);

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		DC_UNLOCK(sc);
		return;
	}

	sc->rxcycles = count;
	dc_rxeof(sc);
	dc_txeof(sc);
	if (!IFQ_IS_EMPTY(&ifp->if_snd) &&
	    !(ifp->if_drv_flags & IFF_DRV_OACTIVE))
		dc_start_locked(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int32_t	status;

		status = CSR_READ_4(sc, DC_ISR);
		status &= (DC_ISR_RX_WATDOGTIMEO | DC_ISR_RX_NOBUF |
			DC_ISR_TX_NOBUF | DC_ISR_TX_IDLE | DC_ISR_TX_UNDERRUN |
			DC_ISR_BUS_ERR);
		if (!status) {
			DC_UNLOCK(sc);
			return;
		}
		/* ack what we have */
		CSR_WRITE_4(sc, DC_ISR, status);

		if (status & (DC_ISR_RX_WATDOGTIMEO | DC_ISR_RX_NOBUF)) {
			u_int32_t r = CSR_READ_4(sc, DC_FRAMESDISCARDED);
			ifp->if_ierrors += (r & 0xffff) + ((r >> 17) & 0x7ff);

			if (dc_rx_resync(sc))
				dc_rxeof(sc);
		}
		/* restart transmit unit if necessary */
		if (status & DC_ISR_TX_IDLE && sc->dc_cdata.dc_tx_cnt)
			CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

		if (status & DC_ISR_TX_UNDERRUN)
			dc_tx_underrun(sc);

		if (status & DC_ISR_BUS_ERR) {
			if_printf(ifp, "%s: bus error\n", __func__);
			dc_reset(sc);
			dc_init_locked(sc);
		}
	}
	DC_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
dc_intr(void *arg)
{
	struct dc_softc *sc;
	struct ifnet *ifp;
	u_int32_t status;

	sc = arg;

	if (sc->suspended)
		return;

	if ((CSR_READ_4(sc, DC_ISR) & DC_INTRS) == 0)
		return;

	DC_LOCK(sc);
	ifp = sc->dc_ifp;
#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		DC_UNLOCK(sc);
		return;
	}
#endif

	/* Suppress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		if (CSR_READ_4(sc, DC_ISR) & DC_INTRS)
			dc_stop(sc);
		DC_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, DC_IMR, 0x00000000);

	while (((status = CSR_READ_4(sc, DC_ISR)) & DC_INTRS) &&
	    status != 0xFFFFFFFF &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {

		CSR_WRITE_4(sc, DC_ISR, status);

		if (status & DC_ISR_RX_OK) {
			int		curpkts;
			curpkts = ifp->if_ipackets;
			dc_rxeof(sc);
			if (curpkts == ifp->if_ipackets) {
				while (dc_rx_resync(sc))
					dc_rxeof(sc);
			}
		}

		if (status & (DC_ISR_TX_OK | DC_ISR_TX_NOBUF))
			dc_txeof(sc);

		if (status & DC_ISR_TX_IDLE) {
			dc_txeof(sc);
			if (sc->dc_cdata.dc_tx_cnt) {
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);
				CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & DC_ISR_TX_UNDERRUN)
			dc_tx_underrun(sc);

		if ((status & DC_ISR_RX_WATDOGTIMEO)
		    || (status & DC_ISR_RX_NOBUF)) {
			int		curpkts;
			curpkts = ifp->if_ipackets;
			dc_rxeof(sc);
			if (curpkts == ifp->if_ipackets) {
				while (dc_rx_resync(sc))
					dc_rxeof(sc);
			}
		}

		if (status & DC_ISR_BUS_ERR) {
			dc_reset(sc);
			dc_init_locked(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, DC_IMR, DC_INTRS);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		dc_start_locked(ifp);

	DC_UNLOCK(sc);
}

static void
dc_dma_map_txbuf(arg, segs, nseg, mapsize, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg;
	bus_size_t mapsize;
	int error;
{
	struct dc_softc *sc;
	struct dc_desc *f;
	int cur, first, frag, i;

	sc = arg;
	if (error)
		return;

	first = cur = frag = sc->dc_cdata.dc_tx_prod;
	for (i = 0; i < nseg; i++) {
		if ((sc->dc_flags & DC_TX_ADMTEK_WAR) &&
		    (frag == (DC_TX_LIST_CNT - 1)) &&
		    (first != sc->dc_cdata.dc_tx_first)) {
			bus_dmamap_unload(sc->dc_mtag,
			    sc->dc_cdata.dc_tx_map[first]);
			sc->dc_cdata.dc_tx_err = ENOBUFS;
			return;
		}

		f = &sc->dc_ldata->dc_tx_list[frag];
		f->dc_ctl = htole32(DC_TXCTL_TLINK | segs[i].ds_len);
		if (i == 0) {
			f->dc_status = 0;
			f->dc_ctl |= htole32(DC_TXCTL_FIRSTFRAG);
		} else
			f->dc_status = htole32(DC_TXSTAT_OWN);
		f->dc_data = htole32(segs[i].ds_addr);
		cur = frag;
		DC_INC(frag, DC_TX_LIST_CNT);
	}

	sc->dc_cdata.dc_tx_err = 0;
	sc->dc_cdata.dc_tx_prod = frag;
	sc->dc_cdata.dc_tx_cnt += nseg;
	sc->dc_ldata->dc_tx_list[cur].dc_ctl |= htole32(DC_TXCTL_LASTFRAG);
	sc->dc_cdata.dc_tx_chain[cur] = sc->dc_cdata.dc_tx_mapping;
	if (sc->dc_flags & DC_TX_INTR_FIRSTFRAG)
		sc->dc_ldata->dc_tx_list[first].dc_ctl |=
		    htole32(DC_TXCTL_FINT);
	if (sc->dc_flags & DC_TX_INTR_ALWAYS)
		sc->dc_ldata->dc_tx_list[cur].dc_ctl |= htole32(DC_TXCTL_FINT);
	if (sc->dc_flags & DC_TX_USE_TX_INTR && sc->dc_cdata.dc_tx_cnt > 64)
		sc->dc_ldata->dc_tx_list[cur].dc_ctl |= htole32(DC_TXCTL_FINT);
	sc->dc_ldata->dc_tx_list[first].dc_status = htole32(DC_TXSTAT_OWN);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
dc_encap(struct dc_softc *sc, struct mbuf **m_head)
{
	struct mbuf *m;
	int error, idx, chainlen = 0;

	/*
	 * If there's no way we can send any packets, return now.
	 */
	if (DC_TX_LIST_CNT - sc->dc_cdata.dc_tx_cnt <= DC_TX_LIST_RSVD)
		return (ENOBUFS);

	/*
	 * Count the number of frags in this chain to see if
	 * we need to m_defrag.  Since the descriptor list is shared
	 * by all packets, we'll m_defrag long chains so that they
	 * do not use up the entire list, even if they would fit.
	 */
	for (m = *m_head; m != NULL; m = m->m_next)
		chainlen++;

	m = NULL;
	if ((sc->dc_flags & DC_TX_COALESCE && ((*m_head)->m_next != NULL ||
	    sc->dc_flags & DC_TX_ALIGN)) || (chainlen > DC_TX_LIST_CNT / 4) ||
	    (DC_TX_LIST_CNT - (chainlen + sc->dc_cdata.dc_tx_cnt) <=
	    DC_TX_LIST_RSVD)) {
		m = m_defrag(*m_head, M_DONTWAIT);
		if (m == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;
	}
	idx = sc->dc_cdata.dc_tx_prod;
	sc->dc_cdata.dc_tx_mapping = *m_head;
	error = bus_dmamap_load_mbuf(sc->dc_mtag, sc->dc_cdata.dc_tx_map[idx],
	    *m_head, dc_dma_map_txbuf, sc, 0);
	if (error != 0 || sc->dc_cdata.dc_tx_err != 0) {
		if (m != NULL) {
			m_freem(m);
			*m_head = NULL;
		}
		return (error != 0 ? error : sc->dc_cdata.dc_tx_err);
	}
	bus_dmamap_sync(sc->dc_mtag, sc->dc_cdata.dc_tx_map[idx],
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->dc_ltag, sc->dc_lmap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void
dc_start(struct ifnet *ifp)
{
	struct dc_softc *sc;

	sc = ifp->if_softc;
	DC_LOCK(sc);
	dc_start_locked(ifp);
	DC_UNLOCK(sc);
}

static void
dc_start_locked(struct ifnet *ifp)
{
	struct dc_softc *sc;
	struct mbuf *m_head = NULL;
	unsigned int queued = 0;
	int idx;

	sc = ifp->if_softc;

	DC_LOCK_ASSERT(sc);

	if (!sc->dc_link && ifp->if_snd.ifq_len < 10)
		return;

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	idx = sc->dc_cdata.dc_tx_first = sc->dc_cdata.dc_tx_prod;

	while (sc->dc_cdata.dc_tx_chain[idx] == NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (dc_encap(sc, &m_head)) {
			if (m_head == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		idx = sc->dc_cdata.dc_tx_prod;

		queued++;
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);

		if (sc->dc_flags & DC_TX_ONE) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
	}

	if (queued > 0) {
		/* Transmit */
		if (!(sc->dc_flags & DC_TX_POLL))
			CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		sc->dc_wdog_timer = 5;
	}
}

static void
dc_init(void *xsc)
{
	struct dc_softc *sc = xsc;

	DC_LOCK(sc);
	dc_init_locked(sc);
	DC_UNLOCK(sc);
}

static void
dc_init_locked(struct dc_softc *sc)
{
	struct ifnet *ifp = sc->dc_ifp;
	struct mii_data *mii;

	DC_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->dc_miibus);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	dc_stop(sc);
	dc_reset(sc);

	/*
	 * Set cache alignment and burst length.
	 */
	if (DC_IS_ASIX(sc) || DC_IS_DAVICOM(sc))
		CSR_WRITE_4(sc, DC_BUSCTL, 0);
	else
		CSR_WRITE_4(sc, DC_BUSCTL, DC_BUSCTL_MRME | DC_BUSCTL_MRLE);
	/*
	 * Evenly share the bus between receive and transmit process.
	 */
	if (DC_IS_INTEL(sc))
		DC_SETBIT(sc, DC_BUSCTL, DC_BUSCTL_ARBITRATION);
	if (DC_IS_DAVICOM(sc) || DC_IS_INTEL(sc)) {
		DC_SETBIT(sc, DC_BUSCTL, DC_BURSTLEN_USECA);
	} else {
		DC_SETBIT(sc, DC_BUSCTL, DC_BURSTLEN_16LONG);
	}
	if (sc->dc_flags & DC_TX_POLL)
		DC_SETBIT(sc, DC_BUSCTL, DC_TXPOLL_1);
	switch(sc->dc_cachesize) {
	case 32:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_32LONG);
		break;
	case 16:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_16LONG);
		break;
	case 8:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_8LONG);
		break;
	case 0:
	default:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_NONE);
		break;
	}

	if (sc->dc_flags & DC_TX_STORENFWD)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
	else {
		if (sc->dc_txthresh > DC_TXTHRESH_MAX) {
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
		} else {
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
			DC_SETBIT(sc, DC_NETCFG, sc->dc_txthresh);
		}
	}

	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_NO_RXCRC);
	DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_TX_BACKOFF);

	if (DC_IS_MACRONIX(sc) || DC_IS_PNICII(sc)) {
		/*
		 * The app notes for the 98713 and 98715A say that
		 * in order to have the chips operate properly, a magic
		 * number must be written to CSR16. Macronix does not
		 * document the meaning of these bits so there's no way
		 * to know exactly what they do. The 98713 has a magic
		 * number all its own; the rest all use a different one.
		 */
		DC_CLRBIT(sc, DC_MX_MAGICPACKET, 0xFFFF0000);
		if (sc->dc_type == DC_TYPE_98713)
			DC_SETBIT(sc, DC_MX_MAGICPACKET, DC_MX_MAGIC_98713);
		else
			DC_SETBIT(sc, DC_MX_MAGICPACKET, DC_MX_MAGIC_98715);
	}

	if (DC_IS_XIRCOM(sc)) {
		/*
		 * setup General Purpose Port mode and data so the tulip
		 * can talk to the MII.
		 */
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_WRITE_EN | DC_SIAGP_INT1_EN |
			   DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_INT1_EN |
			   DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
	}

	DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_TX_THRESH);
	DC_SETBIT(sc, DC_NETCFG, DC_TXTHRESH_MIN);

	/* Init circular RX list. */
	if (dc_list_rx_init(sc) == ENOBUFS) {
		device_printf(sc->dc_dev,
		    "initialization failed: no memory for rx buffers\n");
		dc_stop(sc);
		return;
	}

	/*
	 * Init TX descriptors.
	 */
	dc_list_tx_init(sc);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, DC_RXADDR, DC_RXDESC(sc, 0));
	CSR_WRITE_4(sc, DC_TXADDR, DC_TXDESC(sc, 0));

	/*
	 * Enable interrupts.
	 */
#ifdef DEVICE_POLLING
	/*
	 * ... but only if we are not polling, and make sure they are off in
	 * the case of polling. Some cards (e.g. fxp) turn interrupts on
	 * after a reset.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		CSR_WRITE_4(sc, DC_IMR, 0x00000000);
	else
#endif
	CSR_WRITE_4(sc, DC_IMR, DC_INTRS);
	CSR_WRITE_4(sc, DC_ISR, 0xFFFFFFFF);

	/* Enable transmitter. */
	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);

	/*
	 * If this is an Intel 21143 and we're not using the
	 * MII port, program the LED control pins so we get
	 * link and activity indications.
	 */
	if (sc->dc_flags & DC_TULIP_LEDS) {
		CSR_WRITE_4(sc, DC_WATCHDOG,
		    DC_WDOG_CTLWREN | DC_WDOG_LINK | DC_WDOG_ACTIVITY);
		CSR_WRITE_4(sc, DC_WATCHDOG, 0);
	}

	/*
	 * Load the RX/multicast filter. We do this sort of late
	 * because the filter programming scheme on the 21143 and
	 * some clones requires DMAing a setup frame via the TX
	 * engine, and we need the transmitter enabled for that.
	 */
	dc_setfilt(sc);

	/* Enable receiver. */
	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ON);
	CSR_WRITE_4(sc, DC_RXSTART, 0xFFFFFFFF);

	mii_mediachg(mii);
	dc_setcfg(sc, sc->dc_if_media);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	/* Don't start the ticker if this is a homePNA link. */
	if (IFM_SUBTYPE(mii->mii_media.ifm_media) == IFM_HPNA_1)
		sc->dc_link = 1;
	else {
		if (sc->dc_flags & DC_21143_NWAY)
			callout_reset(&sc->dc_stat_ch, hz/10, dc_tick, sc);
		else
			callout_reset(&sc->dc_stat_ch, hz, dc_tick, sc);
	}

	sc->dc_wdog_timer = 0;
	callout_reset(&sc->dc_wdog_ch, hz, dc_watchdog, sc);
}

/*
 * Set media options.
 */
static int
dc_ifmedia_upd(struct ifnet *ifp)
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->dc_miibus);
	DC_LOCK(sc);
	mii_mediachg(mii);
	ifm = &mii->mii_media;

	if (DC_IS_DAVICOM(sc) &&
	    IFM_SUBTYPE(ifm->ifm_media) == IFM_HPNA_1)
		dc_setcfg(sc, ifm->ifm_media);
	else
		sc->dc_link = 0;
	DC_UNLOCK(sc);

	return (0);
}

/*
 * Report current media status.
 */
static void
dc_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->dc_miibus);
	DC_LOCK(sc);
	mii_pollstat(mii);
	ifm = &mii->mii_media;
	if (DC_IS_DAVICOM(sc)) {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_HPNA_1) {
			ifmr->ifm_active = ifm->ifm_media;
			ifmr->ifm_status = 0;
			DC_UNLOCK(sc);
			return;
		}
	}
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	DC_UNLOCK(sc);
}

static int
dc_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct dc_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		DC_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			int need_setfilt = (ifp->if_flags ^ sc->dc_if_flags) &
				(IFF_PROMISC | IFF_ALLMULTI);

			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if (need_setfilt)
					dc_setfilt(sc);
			} else {
				sc->dc_txthresh = 0;
				dc_init_locked(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				dc_stop(sc);
		}
		sc->dc_if_flags = ifp->if_flags;
		DC_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		DC_LOCK(sc);
		dc_setfilt(sc);
		DC_UNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->dc_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
#ifdef DEVICE_POLLING
		if (ifr->ifr_reqcap & IFCAP_POLLING &&
		    !(ifp->if_capenable & IFCAP_POLLING)) {
			error = ether_poll_register(dc_poll, ifp);
			if (error)
				return(error);
			DC_LOCK(sc);
			/* Disable interrupts */
			CSR_WRITE_4(sc, DC_IMR, 0x00000000);
			ifp->if_capenable |= IFCAP_POLLING;
			DC_UNLOCK(sc);
			return (error);
			
		}
		if (!(ifr->ifr_reqcap & IFCAP_POLLING) &&
		    ifp->if_capenable & IFCAP_POLLING) {
			error = ether_poll_deregister(ifp);
			/* Enable interrupts. */
			DC_LOCK(sc);
			CSR_WRITE_4(sc, DC_IMR, DC_INTRS);
			ifp->if_capenable &= ~IFCAP_POLLING;
			DC_UNLOCK(sc);
			return (error);
		}
#endif /* DEVICE_POLLING */
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
dc_watchdog(void *xsc)
{
	struct dc_softc *sc = xsc;
	struct ifnet *ifp;

	DC_LOCK_ASSERT(sc);

	if (sc->dc_wdog_timer == 0 || --sc->dc_wdog_timer != 0) {
		callout_reset(&sc->dc_wdog_ch, hz, dc_watchdog, sc);
		return;
	}

	ifp = sc->dc_ifp;
	ifp->if_oerrors++;
	device_printf(sc->dc_dev, "watchdog timeout\n");

	dc_stop(sc);
	dc_reset(sc);
	dc_init_locked(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		dc_start_locked(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
dc_stop(struct dc_softc *sc)
{
	struct ifnet *ifp;
	struct dc_list_data *ld;
	struct dc_chain_data *cd;
	int i;
	u_int32_t ctl;

	DC_LOCK_ASSERT(sc);

	ifp = sc->dc_ifp;
	ld = sc->dc_ldata;
	cd = &sc->dc_cdata;

	callout_stop(&sc->dc_stat_ch);
	callout_stop(&sc->dc_wdog_ch);
	sc->dc_wdog_timer = 0;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_RX_ON | DC_NETCFG_TX_ON));
	CSR_WRITE_4(sc, DC_IMR, 0x00000000);
	CSR_WRITE_4(sc, DC_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, DC_RXADDR, 0x00000000);
	sc->dc_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		if (cd->dc_rx_chain[i] != NULL) {
			m_freem(cd->dc_rx_chain[i]);
			cd->dc_rx_chain[i] = NULL;
		}
	}
	bzero(&ld->dc_rx_list, sizeof(ld->dc_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < DC_TX_LIST_CNT; i++) {
		if (cd->dc_tx_chain[i] != NULL) {
			ctl = le32toh(ld->dc_tx_list[i].dc_ctl);
			if ((ctl & DC_TXCTL_SETUP) ||
			    !(ctl & DC_TXCTL_LASTFRAG)) {
				cd->dc_tx_chain[i] = NULL;
				continue;
			}
			bus_dmamap_unload(sc->dc_mtag, cd->dc_tx_map[i]);
			m_freem(cd->dc_tx_chain[i]);
			cd->dc_tx_chain[i] = NULL;
		}
	}
	bzero(&ld->dc_tx_list, sizeof(ld->dc_tx_list));
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
dc_suspend(device_t dev)
{
	struct dc_softc *sc;

	sc = device_get_softc(dev);
	DC_LOCK(sc);
	dc_stop(sc);
	sc->suspended = 1;
	DC_UNLOCK(sc);

	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int
dc_resume(device_t dev)
{
	struct dc_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->dc_ifp;

	/* reinitialize interface if necessary */
	DC_LOCK(sc);
	if (ifp->if_flags & IFF_UP)
		dc_init_locked(sc);

	sc->suspended = 0;
	DC_UNLOCK(sc);

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
dc_shutdown(device_t dev)
{
	struct dc_softc *sc;

	sc = device_get_softc(dev);

	DC_LOCK(sc);
	dc_stop(sc);
	DC_UNLOCK(sc);
}
