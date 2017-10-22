/*	$NetBSD: usb_quirks.c,v 1.50 2004/06/23 02:30:52 mycroft Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/usb/usb_quirks.c 175149 2008-01-07 23:12:45Z brix $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/usb/usb.h>

#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
extern int usbdebug;
#endif

#define ANY 0xffff

static const struct usbd_quirk_entry {
	u_int16_t idVendor;
	u_int16_t idProduct;
	u_int16_t bcdDevice;
	struct usbd_quirks quirks;
} usb_quirks[] = {
 { USB_VENDOR_INSIDEOUT, USB_PRODUCT_INSIDEOUT_EDGEPORT4,
   						    0x094, { UQ_SWAP_UNICODE}},
 { USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502,	    0x0a2, { UQ_BAD_ADC }},
 { USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502,	    0x0a2, { UQ_AU_NO_XU }},
 { USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ADA70,	    0x103, { UQ_BAD_ADC }},
 { USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ASC495,      0x000, { UQ_BAD_AUDIO }},
 { USB_VENDOR_QTRONIX, USB_PRODUCT_QTRONIX_980N,    0x110, { UQ_SPUR_BUT_UP }},
 { USB_VENDOR_ALCOR2, USB_PRODUCT_ALCOR2_KBD_HUB,   0x001, { UQ_SPUR_BUT_UP }},
 { USB_VENDOR_MCT, USB_PRODUCT_MCT_HUB0100,         0x102, { UQ_BUS_POWERED }},
 { USB_VENDOR_MCT, USB_PRODUCT_MCT_USB232,          0x102, { UQ_BUS_POWERED }},
 { USB_VENDOR_TI, USB_PRODUCT_TI_UTUSB41,	    0x110, { UQ_POWER_CLAIM }},
 { USB_VENDOR_TELEX, USB_PRODUCT_TELEX_MIC1,	    0x009, { UQ_AU_NO_FRAC }},
 { USB_VENDOR_SILICONPORTALS, USB_PRODUCT_SILICONPORTALS_YAPPHONE,
   						    0x100, { UQ_AU_INP_ASYNC }},
 { USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_UN53B, ANY, { UQ_NO_STRINGS }},
 /* XXX These should have a revision number, but I don't know what they are. */
 { USB_VENDOR_HP, USB_PRODUCT_HP_895C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_880C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_815C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_810C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_830C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_1220C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_XEROX, USB_PRODUCT_XEROX_WCM15,	    ANY,   { UQ_BROKEN_BIDIR }},
 /* MS keyboards do weird things */
 { USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_WLNOTEBOOK,
   ANY, { UQ_MS_BAD_CLASS | UQ_MS_LEADING_BYTE }},
 { USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_WLNOTEBOOK2,
   ANY, { UQ_MS_BAD_CLASS | UQ_MS_LEADING_BYTE }},
 { USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_WLINTELLIMOUSE,
   ANY, { UQ_MS_LEADING_BYTE }},

 /* Devices which should be ignored by uhid */
 { USB_VENDOR_APC, USB_PRODUCT_APC_UPS,
	ANY, { UQ_HID_IGNORE }},
 { USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F6C550AVR,
	ANY, { UQ_HID_IGNORE }},
 { USB_VENDOR_DELORME, USB_PRODUCT_DELORME_EARTHMATE,
	ANY, { UQ_HID_IGNORE }},
 { USB_VENDOR_ITUNERNET, USB_PRODUCT_ITUNERNET_USBLCD2X20,
	ANY, { UQ_HID_IGNORE }},
 { USB_VENDOR_MGE, USB_PRODUCT_MGE_UPS1,
	ANY, { UQ_HID_IGNORE }},
 { USB_VENDOR_MGE, USB_PRODUCT_MGE_UPS2,
	ANY, { UQ_HID_IGNORE }},

 /* Devices which should be ignored by both ukbd and uhid */
 { USB_VENDOR_CYPRESS, USB_PRODUCT_CYPRESS_WISPY1A,
	ANY, { UQ_KBD_IGNORE }},
 { USB_VENDOR_METAGEEK, USB_PRODUCT_METAGEEK_WISPY1B,
	ANY, { UQ_KBD_IGNORE }},
 { USB_VENDOR_METAGEEK, USB_PRODUCT_METAGEEK_WISPY24X,
	ANY, { UQ_KBD_IGNORE }},
 { 0, 0, 0, { 0 } }
};

const struct usbd_quirks usbd_no_quirk = { 0 };

const struct usbd_quirks *
usbd_find_quirk(usb_device_descriptor_t *d)
{
	const struct usbd_quirk_entry *t;
	u_int16_t vendor = UGETW(d->idVendor);
	u_int16_t product = UGETW(d->idProduct);
	u_int16_t revision = UGETW(d->bcdDevice);

	for (t = usb_quirks; t->idVendor != 0; t++) {
		if (t->idVendor  == vendor &&
		    t->idProduct == product &&
		    (t->bcdDevice == ANY || t->bcdDevice == revision))
			break;
	}
#ifdef USB_DEBUG
	if (usbdebug && t->quirks.uq_flags)
		printf("usbd_find_quirk 0x%04x/0x%04x/%x: %d\n",
			  UGETW(d->idVendor), UGETW(d->idProduct),
			  UGETW(d->bcdDevice), t->quirks.uq_flags);
#endif
	return (&t->quirks);
}
