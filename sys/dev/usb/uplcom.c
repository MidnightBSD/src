/*	$NetBSD: uplcom.c,v 1.21 2001/11/13 06:24:56 lukem Exp $	*/

/*-
 * Copyright (c) 2001-2003, 2005 Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/usb/uplcom.c 170960 2007-06-20 05:11:37Z imp $");

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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

/*
 * This driver supports several USB-to-RS232 serial adapters driven by
 * Prolific PL-2303, PL-2303X and probably PL-2303HX USB-to-RS232
 * bridge chip.  The adapters are sold under many different brand
 * names.
 *
 * Datasheets are available at Prolific www site at
 * http://www.prolific.com.tw.  The datasheets don't contain full
 * programming information for the chip.
 *
 * PL-2303HX is probably programmed the same as PL-2303X.
 *
 * There are several differences between PL-2303 and PL-2303(H)X.
 * PL-2303(H)X can do higher bitrate in bulk mode, has _probably_
 * different command for controlling CRTSCTS and needs special
 * sequence of commands for initialization which aren't also
 * documented in the datasheet.
 */

#include "opt_uplcom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/serial.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

SYSCTL_NODE(_hw_usb, OID_AUTO, uplcom, CTLFLAG_RW, 0, "USB uplcom");
#ifdef USB_DEBUG
static int	uplcomdebug = 0;
SYSCTL_INT(_hw_usb_uplcom, OID_AUTO, debug, CTLFLAG_RW,
	   &uplcomdebug, 0, "uplcom debug level");

#define DPRINTFN(n, x)	do { \
				if (uplcomdebug > (n)) \
					printf x; \
			} while (0)
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define UPLCOM_MODVER			1	/* module version */

#define	UPLCOM_CONFIG_INDEX		0
#define	UPLCOM_IFACE_INDEX		0
#define	UPLCOM_SECOND_IFACE_INDEX	1

#ifndef UPLCOM_INTR_INTERVAL
#define UPLCOM_INTR_INTERVAL		100	/* ms */
#endif

#define	UPLCOM_SET_REQUEST		0x01
#define	UPLCOM_SET_CRTSCTS		0x41
#define	UPLCOM_SET_CRTSCTS_PL2303X	0x61
#define RSAQ_STATUS_CTS			0x80
#define RSAQ_STATUS_DSR			0x02
#define RSAQ_STATUS_DCD			0x01

#define TYPE_PL2303			0
#define TYPE_PL2303X			1

struct	uplcom_softc {
	struct ucom_softc	sc_ucom;

	int			sc_iface_number;	/* interface number */

	usbd_interface_handle	sc_intr_iface;	/* interrupt interface */
	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
	int			sc_isize;

	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */
	u_char			sc_status;

	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* uplcom status register */

	int			sc_chiptype;	/* Type of chip */

	struct task		sc_task;
};

/*
 * These are the maximum number of bytes transferred per frame.
 * The output buffer size cannot be increased due to the size encoding.
 */
#define UPLCOMIBUFSIZE 256
#define UPLCOMOBUFSIZE 256

static	usbd_status uplcom_reset(struct uplcom_softc *);
static	usbd_status uplcom_set_line_coding(struct uplcom_softc *,
					   usb_cdc_line_state_t *);
static	usbd_status uplcom_set_crtscts(struct uplcom_softc *);
static	void uplcom_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

static	void uplcom_set(void *, int, int, int);
static	void uplcom_dtr(struct uplcom_softc *, int);
static	void uplcom_rts(struct uplcom_softc *, int);
static	void uplcom_break(struct uplcom_softc *, int);
static	void uplcom_set_line_state(struct uplcom_softc *);
static	void uplcom_get_status(void *, int, u_char *, u_char *);
#if 0 /* TODO */
static	int  uplcom_ioctl(void *, int, u_long, caddr_t, int, usb_proc_ptr);
#endif
static	int  uplcom_param(void *, int, struct termios *);
static	int  uplcom_open(void *, int);
static	void uplcom_close(void *, int);
static	void uplcom_notify(void *, int);

struct ucom_callback uplcom_callback = {
	uplcom_get_status,
	uplcom_set,
	uplcom_param,
	NULL, /* uplcom_ioctl, TODO */
	uplcom_open,
	uplcom_close,
	NULL,
	NULL
};

static const struct uplcom_product {
	uint16_t	vendor;
	uint16_t	product;
	int32_t		release;	 /* release is a 16bit entity,
					  * if -1 is specified we "don't care"
					  * This is a floor value.  The table
					  * must have newer revs before older
					  * revs (and -1 last).
					  */
	char		chiptype;
} uplcom_products [] = {
	{ USB_VENDOR_RADIOSHACK, USB_PRODUCT_RADIOSHACK_USBCABLE, -1, TYPE_PL2303 },

	/* I/O DATA USB-RSAQ */
	{ USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBRSAQ, -1, TYPE_PL2303 },
	/* Prolific Pharos */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PHAROS, -1, TYPE_PL2303 },
	/* I/O DATA USB-RSAQ2 */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_RSAQ2, -1, TYPE_PL2303 },
	/* I/O DATA USB-RSAQ3 */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_RSAQ3, -1, TYPE_PL2303X },
	/* Willcom W-SIM*/
	{ USB_VENDOR_PROLIFIC2, USB_PRODUCT_PROLIFIC2_WSIM, -1, TYPE_PL2303X},
	/* PLANEX USB-RS232 URS-03 */
	{ USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC232A, -1, TYPE_PL2303 },
	/* ST Lab USB-SERIAL-4 */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PL2303,
	  0x300, TYPE_PL2303X },
	/* IOGEAR/ATEN UC-232A (also ST Lab USB-SERIAL-1) */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PL2303, -1, TYPE_PL2303 },
	/* TDK USB-PHS Adapter UHA6400 */
	{ USB_VENDOR_TDK, USB_PRODUCT_TDK_UHA6400, -1, TYPE_PL2303 },
	/* RATOC REX-USB60 */
	{ USB_VENDOR_RATOC, USB_PRODUCT_RATOC_REXUSB60, -1, TYPE_PL2303 },
	/* ELECOM UC-SGT */
	{ USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_UCSGT, -1, TYPE_PL2303 },
	{ USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_UCSGT0, -1, TYPE_PL2303 },
	/* Sagem USB-Serial Controller */
	{ USB_VENDOR_SAGEM, USB_PRODUCT_SAGEM_USBSERIAL, -1, TYPE_PL2303X },
	/* Sony Ericsson USB Cable */
	{ USB_VENDOR_SONYERICSSON, USB_PRODUCT_SONYERICSSON_DCU10,
	  -1,TYPE_PL2303 },
	/* SOURCENEXT KeikaiDenwa 8 */
	{ USB_VENDOR_SOURCENEXT, USB_PRODUCT_SOURCENEXT_KEIKAI8,
	  -1, TYPE_PL2303 },
	/* SOURCENEXT KeikaiDenwa 8 with charger */
	{ USB_VENDOR_SOURCENEXT, USB_PRODUCT_SOURCENEXT_KEIKAI8_CHG,
	  -1, TYPE_PL2303 },
	/* HAL Corporation Crossam2+USB */
	{ USB_VENDOR_HAL, USB_PRODUCT_HAL_IMR001, -1, TYPE_PL2303 },
	/* Sitecom USB to Serial */
	{ USB_VENDOR_SITECOM, USB_PRODUCT_SITECOM_SERIAL, -1, TYPE_PL2303 },
	/* Tripp-Lite U209-000-R */
	{ USB_VENDOR_TRIPPLITE, USB_PRODUCT_TRIPPLITE_U209, -1, TYPE_PL2303X },
	{ 0, 0 }
};

static device_probe_t uplcom_match;
static device_attach_t uplcom_attach;
static device_detach_t uplcom_detach;

static device_method_t uplcom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uplcom_match),
	DEVMETHOD(device_attach, uplcom_attach),
	DEVMETHOD(device_detach, uplcom_detach),
	{ 0, 0 }
};

static driver_t uplcom_driver = {
	"ucom",
	uplcom_methods,
	sizeof (struct uplcom_softc)
};

DRIVER_MODULE(uplcom, uhub, uplcom_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uplcom, usb, 1, 1, 1);
MODULE_DEPEND(uplcom, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(uplcom, UPLCOM_MODVER);

static int	uplcominterval = UPLCOM_INTR_INTERVAL;

static int
sysctl_hw_usb_uplcom_interval(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = uplcominterval;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (0 < val && val <= 1000)
		uplcominterval = val;
	else
		err = EINVAL;

	return (err);
}

SYSCTL_PROC(_hw_usb_uplcom, OID_AUTO, interval, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(int), sysctl_hw_usb_uplcom_interval,
	    "I", "uplcom interrupt pipe interval");

static int
uplcom_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	int i;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	for (i = 0; uplcom_products[i].vendor != 0; i++) {
		if (uplcom_products[i].vendor == uaa->vendor &&
		    uplcom_products[i].product == uaa->product &&
		    (uplcom_products[i].release <= uaa->release ||
		     uplcom_products[i].release == -1)) {
			return (UMATCH_VENDOR_PRODUCT);
		}
	}
	return (UMATCH_NONE);
}

static int
uplcom_attach(device_t self)
{
	struct uplcom_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	struct ucom_softc *ucom;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	const char *devname;
	usbd_status err;
	int i;

	ucom = &sc->sc_ucom;
	ucom->sc_dev = self;
	ucom->sc_udev = dev;
	ucom->sc_iface = uaa->iface;

	devname = device_get_nameunit(ucom->sc_dev);

	DPRINTF(("uplcom attach: sc = %p\n", sc));

	/* determine chip type */
	for (i = 0; uplcom_products[i].vendor != 0; i++) {
		if (uplcom_products[i].vendor == uaa->vendor &&
		    uplcom_products[i].product == uaa->product &&
		    (uplcom_products[i].release == uaa->release ||
		     uplcom_products[i].release == -1)) {
			sc->sc_chiptype = uplcom_products[i].chiptype;
			break;
		}
	}

	/*
	 * check we found the device - attach should have ensured we
	 * don't get here without matching device
	 */
	if (uplcom_products[i].vendor == 0) {
		printf("%s: didn't match\n", devname);
		ucom->sc_dying = 1;
		goto error;
	}

#ifdef USB_DEBUG
	/* print the chip type */
	if (sc->sc_chiptype == TYPE_PL2303X) {
		DPRINTF(("uplcom_attach: chiptype 2303X\n"));
	} else {
		DPRINTF(("uplcom_attach: chiptype 2303\n"));
	}
#endif

	/* initialize endpoints */
	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UPLCOM_CONFIG_INDEX, 1);
	if (err) {
		printf("%s: failed to set configuration: %s\n",
			devname, usbd_errstr(err));
		ucom->sc_dying = 1;
		goto error;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(ucom->sc_udev);

	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
			device_get_nameunit(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	/* get the (first/common) interface */
	err = usbd_device2interface_handle(dev, UPLCOM_IFACE_INDEX,
					   &ucom->sc_iface);
	if (err) {
		printf("%s: failed to get interface: %s\n",
			devname, usbd_errstr(err));
		ucom->sc_dying = 1;
		goto error;
	}

	/* Find the interrupt endpoints */

	id = usbd_get_interface_descriptor(ucom->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
				device_get_nameunit(ucom->sc_dev), i);
			ucom->sc_dying = 1;
			goto error;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
		}
	}

	if (sc->sc_intr_number == -1) {
		printf("%s: Could not find interrupt in\n",
			device_get_nameunit(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	/* keep interface for interrupt */
	sc->sc_intr_iface = ucom->sc_iface;

	/*
	 * USB-RSAQ1 has two interface
	 *
	 *  USB-RSAQ1       | USB-RSAQ2
	 * -----------------+-----------------
	 * Interface 0      |Interface 0
	 *  Interrupt(0x81) | Interrupt(0x81)
	 * -----------------+ BulkIN(0x02)
	 * Interface 1	    | BulkOUT(0x83)
	 *   BulkIN(0x02)   |
	 *   BulkOUT(0x83)  |
	 */
	if (cdesc->bNumInterface == 2) {
		err = usbd_device2interface_handle(dev,
						   UPLCOM_SECOND_IFACE_INDEX,
						   &ucom->sc_iface);
		if (err) {
			printf("%s: failed to get second interface: %s\n",
				devname, usbd_errstr(err));
			ucom->sc_dying = 1;
			goto error;
		}
	}

	/* Find the bulk{in,out} endpoints */

	id = usbd_get_interface_descriptor(ucom->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
				device_get_nameunit(ucom->sc_dev), i);
			ucom->sc_dying = 1;
			goto error;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkout_no = ed->bEndpointAddress;
		}
	}

	if (ucom->sc_bulkin_no == -1) {
		printf("%s: Could not find data bulk in\n",
			device_get_nameunit(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	if (ucom->sc_bulkout_no == -1) {
		printf("%s: Could not find data bulk out\n",
			device_get_nameunit(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	sc->sc_dtr = sc->sc_rts = -1;
	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = UPLCOMIBUFSIZE;
	ucom->sc_obufsize = UPLCOMOBUFSIZE;
	ucom->sc_ibufsizepad = UPLCOMIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uplcom_callback;

	err = uplcom_reset(sc);

	if (err) {
		printf("%s: reset failed: %s\n",
		       device_get_nameunit(ucom->sc_dev), usbd_errstr(err));
		ucom->sc_dying = 1;
		goto error;
	}

	DPRINTF(("uplcom: in = 0x%x, out = 0x%x, intr = 0x%x\n",
		 ucom->sc_bulkin_no, ucom->sc_bulkout_no, sc->sc_intr_number));

	TASK_INIT(&sc->sc_task, 0, uplcom_notify, sc);
	ucom_attach(&sc->sc_ucom);
	return 0;

error:
	return ENXIO;
}

static int
uplcom_detach(device_t self)
{
	struct uplcom_softc *sc = device_get_softc(self);
	int rv = 0;

	DPRINTF(("uplcom_detach: sc = %p\n", sc));

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}

	sc->sc_ucom.sc_dying = 1;

	rv = ucom_detach(&sc->sc_ucom);

	return (rv);
}

static usbd_status
uplcom_reset(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UPLCOM_SET_REQUEST;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err) {
		printf("%s: uplcom_reset: %s\n",
		       device_get_nameunit(sc->sc_ucom.sc_dev), usbd_errstr(err));
		return (EIO);
	}

	return (0);
}

struct pl2303x_init {
	uint8_t		req_type;
	uint8_t		request;
	uint16_t	value;
	uint16_t	index;
	uint16_t	length;
};

static const struct pl2303x_init pl2303x[] = {
	{ UT_READ_VENDOR_DEVICE,  UPLCOM_SET_REQUEST, 0x8484,    0, 0 },
	{ UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x0404,    0, 0 },
	{ UT_READ_VENDOR_DEVICE,  UPLCOM_SET_REQUEST, 0x8484,    0, 0 },
	{ UT_READ_VENDOR_DEVICE,  UPLCOM_SET_REQUEST, 0x8383,    0, 0 },
	{ UT_READ_VENDOR_DEVICE,  UPLCOM_SET_REQUEST, 0x8484,    0, 0 },
	{ UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x0404,    1, 0 },
	{ UT_READ_VENDOR_DEVICE,  UPLCOM_SET_REQUEST, 0x8484,    0, 0 },
	{ UT_READ_VENDOR_DEVICE,  UPLCOM_SET_REQUEST, 0x8383,    0, 0 },
	{ UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST,      0,    1, 0 },
	{ UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST,      1,    0, 0 },
	{ UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST,      2, 0x44, 0 }
};
#define N_PL2302X_INIT	(sizeof(pl2303x)/sizeof(pl2303x[0]))

static usbd_status
uplcom_pl2303x_init(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;
	int i;

	for (i = 0; i < N_PL2302X_INIT; i++) {
		req.bmRequestType = pl2303x[i].req_type;
		req.bRequest = pl2303x[i].request;
		USETW(req.wValue, pl2303x[i].value);
		USETW(req.wIndex, pl2303x[i].index);
		USETW(req.wLength, pl2303x[i].length);

		err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
		if (err) {
			printf("%s: uplcom_pl2303x_init: %d: %s\n",
				device_get_nameunit(sc->sc_ucom.sc_dev), i,
				usbd_errstr(err));
			return (EIO);
		}
	}

	return (0);
}

static void
uplcom_set_line_state(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	int ls;
	usbd_status err;

	ls = (sc->sc_dtr ? UCDC_LINE_DTR : 0) |
		(sc->sc_rts ? UCDC_LINE_RTS : 0);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, ls);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err)
		printf("%s: uplcom_set_line_status: %s\n",
		       device_get_nameunit(sc->sc_ucom.sc_dev), usbd_errstr(err));
}

static void
uplcom_set(void *addr, int portno, int reg, int onoff)
{
	struct uplcom_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		uplcom_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		uplcom_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		uplcom_break(sc, onoff);
		break;
	default:
		break;
	}
}

static void
uplcom_dtr(struct uplcom_softc *sc, int onoff)
{
	DPRINTF(("uplcom_dtr: onoff = %d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	uplcom_set_line_state(sc);
}

static void
uplcom_rts(struct uplcom_softc *sc, int onoff)
{
	DPRINTF(("uplcom_rts: onoff = %d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	uplcom_set_line_state(sc);
}

static void
uplcom_break(struct uplcom_softc *sc, int onoff)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("uplcom_break: onoff = %d\n", onoff));

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err)
		printf("%s: uplcom_break: %s\n",
		       device_get_nameunit(sc->sc_ucom.sc_dev), usbd_errstr(err));
}

static usbd_status
uplcom_set_crtscts(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("uplcom_set_crtscts: on\n"));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UPLCOM_SET_REQUEST;
	USETW(req.wValue, 0);
	if (sc->sc_chiptype == TYPE_PL2303X)
		USETW(req.wIndex, UPLCOM_SET_CRTSCTS_PL2303X);
	else
		USETW(req.wIndex, UPLCOM_SET_CRTSCTS);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err) {
		printf("%s: uplcom_set_crtscts: %s\n",
		       device_get_nameunit(sc->sc_ucom.sc_dev), usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
uplcom_set_line_coding(struct uplcom_softc *sc, usb_cdc_line_state_t *state)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF((
"uplcom_set_line_coding: rate = %d, fmt = %d, parity = %d bits = %d\n",
		 UGETDW(state->dwDTERate), state->bCharFormat,
		 state->bParityType, state->bDataBits));

	if (memcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("uplcom_set_line_coding: already set\n"));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, state);
	if (err) {
		printf("%s: uplcom_set_line_coding: %s\n",
		       device_get_nameunit(sc->sc_ucom.sc_dev), usbd_errstr(err));
		return (err);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

static const int uplcom_rates[] = {
	75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600, 14400,
	19200, 28800, 38400, 57600, 115200,
	/*
	 * Higher speeds are probably possible. PL2303X supports up to
	 * 6Mb and can set any rate
	 */
	230400, 460800, 614400, 921600,	1228800
};
#define N_UPLCOM_RATES	(sizeof(uplcom_rates)/sizeof(uplcom_rates[0]))

static int
uplcom_param(void *addr, int portno, struct termios *t)
{
	struct uplcom_softc *sc = addr;
	usbd_status err;
	usb_cdc_line_state_t ls;
	int i;

	DPRINTF(("uplcom_param: sc = %p\n", sc));

	/* Check requested baud rate */
	for (i = 0; i < N_UPLCOM_RATES; i++)
		if (uplcom_rates[i] == t->c_ospeed)
			break;
	if (i == N_UPLCOM_RATES) {
		DPRINTF(("uplcom_param: bad baud rate (%d)\n", t->c_ospeed));
		return (EIO);
	}

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}

	err = uplcom_set_line_coding(sc, &ls);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CRTSCTS)) {
		err = uplcom_set_crtscts(sc);
		if (err)
			return (EIO);
	}

	return (0);
}

static int
uplcom_open(void *addr, int portno)
{
	struct uplcom_softc *sc = addr;
	int err;

	if (sc->sc_ucom.sc_dying)
		return (ENXIO);

	DPRINTF(("uplcom_open: sc = %p\n", sc));

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_status = 0; /* clear status bit */
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_intr_iface,
					  sc->sc_intr_number,
					  USBD_SHORT_XFER_OK,
					  &sc->sc_intr_pipe,
					  sc,
					  sc->sc_intr_buf,
					  sc->sc_isize,
					  uplcom_intr,
					  uplcominterval);
		if (err) {
			printf("%s: cannot open interrupt pipe (addr %d)\n",
			       device_get_nameunit(sc->sc_ucom.sc_dev),
			       sc->sc_intr_number);
			return (EIO);
		}
	}

	if (sc->sc_chiptype == TYPE_PL2303X)
		return (uplcom_pl2303x_init(sc));

	return (0);
}

static void
uplcom_close(void *addr, int portno)
{
	struct uplcom_softc *sc = addr;
	int err;

	if (sc->sc_ucom.sc_dying)
		return;

	DPRINTF(("uplcom_close: close\n"));

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: abort interrupt pipe failed: %s\n",
			       device_get_nameunit(sc->sc_ucom.sc_dev),
			       usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: close interrupt pipe failed: %s\n",
			       device_get_nameunit(sc->sc_ucom.sc_dev),
			       usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}

static void
uplcom_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct uplcom_softc *sc = priv;
	u_char *buf = sc->sc_intr_buf;
	u_char pstatus;

	if (sc->sc_ucom.sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		DPRINTF(("%s: uplcom_intr: abnormal status: %s\n",
			device_get_nameunit(sc->sc_ucom.sc_dev),
			usbd_errstr(status)));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	DPRINTF(("%s: uplcom status = %02x\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), buf[8]));

	sc->sc_lsr = sc->sc_msr = 0;
	pstatus = buf[8];
	if (ISSET(pstatus, RSAQ_STATUS_CTS))
		sc->sc_msr |= SER_CTS;
	else
		sc->sc_msr &= ~SER_CTS;
	if (ISSET(pstatus, RSAQ_STATUS_DSR))
		sc->sc_msr |= SER_DSR;
	else
		sc->sc_msr &= ~SER_DSR;
	if (ISSET(pstatus, RSAQ_STATUS_DCD))
		sc->sc_msr |= SER_DCD;
	else
		sc->sc_msr &= ~SER_DCD;

	/* Deferred notifying to the ucom layer */
	taskqueue_enqueue(taskqueue_swi_giant, &sc->sc_task);
}

static void
uplcom_notify(void *arg, int count)
{
	struct uplcom_softc *sc;

	sc = (struct uplcom_softc *)arg;
	if (sc->sc_ucom.sc_dying)
		return;
	ucom_status_change(&sc->sc_ucom);
}

static void
uplcom_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct uplcom_softc *sc = addr;

	DPRINTF(("uplcom_get_status:\n"));

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

#if 0 /* TODO */
static int
uplcom_ioctl(void *addr, int portno, u_long cmd, caddr_t data, int flag,
	     struct thread *p)
{
	struct uplcom_softc *sc = addr;
	int error = 0;

	if (sc->sc_ucom.sc_dying)
		return (EIO);

	DPRINTF(("uplcom_ioctl: cmd = 0x%08lx\n", cmd));

	switch (cmd) {
	case TIOCNOTTY:
	case TIOCMGET:
	case TIOCMSET:
	case USB_GET_CM_OVER_DATA:
	case USB_SET_CM_OVER_DATA:
		break;

	default:
		DPRINTF(("uplcom_ioctl: unknown\n"));
		error = ENOTTY;
		break;
	}

	return (error);
}
#endif
