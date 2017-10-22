/*-
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Copyright (c) 2006
 *      Alfred Perlstein <alfred@freebsd.org>. All rights reserved.
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
 *
 * $FreeBSD: release/7.0.0/sys/dev/usb/if_auereg.h 165902 2007-01-08 23:24:21Z alfred $
 */

/*
 * Register definitions for ADMtek Pegasus AN986 USB to Ethernet
 * chip. The Pegasus uses a total of four USB endpoints: the control
 * endpoint (0), a bulk read endpoint for receiving packets (1),
 * a bulk write endpoint for sending packets (2) and an interrupt
 * endpoint for passing RX and TX status (3). Endpoint 0 is used
 * to read and write the ethernet module's registers. All registers
 * are 8 bits wide.
 *
 * Packet transfer is done in 64 byte chunks. The last chunk in a
 * transfer is denoted by having a length less that 64 bytes. For
 * the RX case, the data includes an optional RX status word.
 */

#ifndef AUEREG_H
#define AUEREG_H

#define AUE_UR_READREG		0xF0
#define AUE_UR_WRITEREG		0xF1

#define AUE_CONFIG_NO		1
#define AUE_IFACE_IDX		0

/*
 * Note that while the ADMtek technically has four
 * endpoints, the control endpoint (endpoint 0) is
 * regarded as special by the USB code and drivers
 * don't have direct access to it. (We access it
 * using usbd_do_request() when reading/writing
 * registers.) Consequently, our endpoint indexes
 * don't match those in the ADMtek Pegasus manual:
 * we consider the RX data endpoint to be index 0
 * and work up from there.
 */
#define AUE_ENDPT_RX		0x0
#define AUE_ENDPT_TX		0x1
#define AUE_ENDPT_INTR		0x2
#define AUE_ENDPT_MAX		0x3

#define AUE_INTR_PKTLEN		0x8

#define AUE_CTL0		0x00
#define AUE_CTL1		0x01
#define AUE_CTL2		0x02
#define AUE_MAR0		0x08
#define AUE_MAR1		0x09
#define AUE_MAR2		0x0A
#define AUE_MAR3		0x0B
#define AUE_MAR4		0x0C
#define AUE_MAR5		0x0D
#define AUE_MAR6		0x0E
#define AUE_MAR7		0x0F
#define AUE_MAR			AUE_MAR0
#define AUE_PAR0		0x10
#define AUE_PAR1		0x11
#define AUE_PAR2		0x12
#define AUE_PAR3		0x13
#define AUE_PAR4		0x14
#define AUE_PAR5		0x15
#define AUE_PAR			AUE_PAR0
#define AUE_PAUSE0		0x18
#define AUE_PAUSE1		0x19
#define AUE_PAUSE		AUE_PAUSE0
#define AUE_RX_FLOWCTL_CNT	0x1A
#define AUE_RX_FLOWCTL_FIFO	0x1B
#define AUE_REG_1D		0x1D
#define AUE_EE_REG		0x20
#define AUE_EE_DATA0		0x21
#define AUE_EE_DATA1		0x22
#define AUE_EE_DATA		AUE_EE_DATA0
#define AUE_EE_CTL		0x23
#define AUE_PHY_ADDR		0x25
#define AUE_PHY_DATA0		0x26
#define AUE_PHY_DATA1		0x27
#define AUE_PHY_DATA		AUE_PHY_DATA0
#define AUE_PHY_CTL		0x28
#define AUE_USB_STS		0x2A
#define AUE_TXSTAT0		0x2B
#define AUE_TXSTAT1		0x2C
#define AUE_TXSTAT		AUE_TXSTAT0
#define AUE_RXSTAT		0x2D
#define AUE_PKTLOST0		0x2E
#define AUE_PKTLOST1		0x2F
#define AUE_PKTLOST		AUE_PKTLOST0

#define AUE_REG_7B		0x7B
#define AUE_GPIO0		0x7E
#define AUE_GPIO1		0x7F
#define AUE_REG_81		0x81

#define AUE_CTL0_INCLUDE_RXCRC	0x01
#define AUE_CTL0_ALLMULTI	0x02
#define AUE_CTL0_STOP_BACKOFF	0x04
#define AUE_CTL0_RXSTAT_APPEND	0x08
#define AUE_CTL0_WAKEON_ENB	0x10
#define AUE_CTL0_RXPAUSE_ENB	0x20
#define AUE_CTL0_RX_ENB		0x40
#define AUE_CTL0_TX_ENB		0x80

#define AUE_CTL1_HOMELAN	0x04
#define AUE_CTL1_RESETMAC	0x08
#define AUE_CTL1_SPEEDSEL	0x10	/* 0 = 10mbps, 1 = 100mbps */
#define AUE_CTL1_DUPLEX		0x20	/* 0 = half, 1 = full */
#define AUE_CTL1_DELAYHOME	0x40

#define AUE_CTL2_EP3_CLR	0x01	/* reading EP3 clrs status regs */
#define AUE_CTL2_RX_BADFRAMES	0x02
#define AUE_CTL2_RX_PROMISC	0x04
#define AUE_CTL2_LOOPBACK	0x08
#define AUE_CTL2_EEPROMWR_ENB	0x10
#define AUE_CTL2_EEPROM_LOAD	0x20

#define AUE_EECTL_WRITE		0x01
#define AUE_EECTL_READ		0x02
#define AUE_EECTL_DONE		0x04

#define AUE_PHYCTL_PHYREG	0x1F
#define AUE_PHYCTL_WRITE	0x20
#define AUE_PHYCTL_READ		0x40
#define AUE_PHYCTL_DONE		0x80

#define AUE_USBSTS_SUSPEND	0x01
#define AUE_USBSTS_RESUME	0x02

#define AUE_TXSTAT0_JABTIMO	0x04
#define AUE_TXSTAT0_CARLOSS	0x08
#define AUE_TXSTAT0_NOCARRIER	0x10
#define AUE_TXSTAT0_LATECOLL	0x20
#define AUE_TXSTAT0_EXCESSCOLL	0x40
#define AUE_TXSTAT0_UNDERRUN	0x80

#define AUE_TXSTAT1_PKTCNT	0x0F
#define AUE_TXSTAT1_FIFO_EMPTY	0x40
#define AUE_TXSTAT1_FIFO_FULL	0x80

#define AUE_RXSTAT_OVERRUN	0x01
#define AUE_RXSTAT_PAUSE	0x02

#define AUE_GPIO_IN0		0x01
#define AUE_GPIO_OUT0		0x02
#define AUE_GPIO_SEL0		0x04
#define AUE_GPIO_IN1		0x08
#define AUE_GPIO_OUT1		0x10
#define AUE_GPIO_SEL1		0x20

struct aue_intrpkt {
	u_int8_t		aue_txstat0;
	u_int8_t		aue_txstat1;
	u_int8_t		aue_rxstat;
	u_int8_t		aue_rxlostpkt0;
	u_int8_t		aue_rxlostpkt1;
	u_int8_t		aue_wakeupstat;
	u_int8_t		aue_rsvd;
};

struct aue_rxpkt {
	u_int16_t		aue_pktlen;
	u_int8_t		aue_rxstat;
};

#define AUE_RXSTAT_MCAST	0x01
#define AUE_RXSTAT_GIANT	0x02
#define AUE_RXSTAT_RUNT		0x04
#define AUE_RXSTAT_CRCERR	0x08
#define AUE_RXSTAT_DRIBBLE	0x10
#define AUE_RXSTAT_MASK		0x1E

#define AUE_INC(x, y)		(x) = (x + 1) % y

struct aue_softc {
#if defined(__FreeBSD__)
#define GET_MII(sc) (device_get_softc((sc)->aue_miibus))
#elif defined(__NetBSD__)
#define GET_MII(sc) (&(sc)->aue_mii)
#elif defined(__OpenBSD__)
#define GET_MII(sc) (&(sc)->aue_mii)
#endif
	struct ifnet		*aue_ifp;
	device_t		aue_dev;
	device_t		aue_miibus;
	usbd_device_handle	aue_udev;
	usbd_interface_handle	aue_iface;
	u_int16_t		aue_vendor;
	u_int16_t		aue_product;
	int			aue_ed[AUE_ENDPT_MAX];
	usbd_pipe_handle	aue_ep[AUE_ENDPT_MAX];
	int			aue_unit;
	u_int8_t		aue_link;
	int			aue_timer;
	int			aue_if_flags;
	struct ue_cdata		aue_cdata;
	struct callout		aue_tick_callout;
	struct usb_taskqueue	aue_taskqueue;
	struct task		aue_task;
	struct mtx		aue_mtx;
	struct sx		aue_sx;
	u_int16_t		aue_flags;
	char			aue_dying;
	struct timeval		aue_rx_notice;
	struct usb_qdat		aue_qdat;
	int			aue_deferedtasks;
};

#if 0
/*
 * Some debug code to make sure we don't take a blocking lock in
 * interrupt context.
 */
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/kdb.h>

#define AUE_DUMPSTATE(tag)	aue_dumpstate(__func__, tag)

static inline void
aue_dumpstate(const char *func, const char *tag)
{
	if ((curthread->td_pflags & TDP_NOSLEEPING) ||
	    (curthread->td_pflags & TDP_ITHREAD)) {
		kdb_backtrace();
		printf("%s: %s sleep: %sok ithread: %s\n", func, tag,
			curthread->td_pflags & TDP_NOSLEEPING ? "not" : "",
			curthread->td_pflags & TDP_ITHREAD ?  "yes" : "no");
	}
}
#else
#define AUE_DUMPSTATE(tag)
#endif

#define AUE_LOCK(_sc)			mtx_lock(&(_sc)->aue_mtx)
#define AUE_UNLOCK(_sc)			mtx_unlock(&(_sc)->aue_mtx)
#define AUE_SXLOCK(_sc)	\
    do { AUE_DUMPSTATE("sxlock"); sx_xlock(&(_sc)->aue_sx); } while(0)
#define AUE_SXUNLOCK(_sc)		sx_xunlock(&(_sc)->aue_sx)
#define AUE_SXASSERTLOCKED(_sc)		sx_assert(&(_sc)->aue_sx, SX_XLOCKED)
#define AUE_SXASSERTUNLOCKED(_sc)	sx_assert(&(_sc)->aue_sx, SX_UNLOCKED)

#define AUE_TIMEOUT		1000
#define AUE_MIN_FRAMELEN	60
#define AUE_INTR_INTERVAL	100 /* ms */

/*
 * These bits are used to notify the task about pending events.
 * The names correspond to the interrupt context routines that would
 * be normally called.  (example: AUE_TASK_WATCHDOG -> aue_watchdog())
 */
#define AUE_TASK_WATCHDOG	0x0001
#define AUE_TASK_TICK		0x0002
#define AUE_TASK_START		0x0004
#define AUE_TASK_RXSTART	0x0008
#define AUE_TASK_RXEOF		0x0010
#define AUE_TASK_TXEOF		0x0020

#define AUE_GIANTLOCK()		mtx_lock(&Giant);
#define AUE_GIANTUNLOCK()	mtx_unlock(&Giant);

#endif /* !AUEREG_H */
