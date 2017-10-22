/*	$NetBSD: usb/uvscom.c,v 1.1 2002/03/19 15:08:42 augustss Exp $	*/
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/usb/uvscom.c 171233 2007-07-05 06:28:46Z imp $");

/*
 * uvscom: SUNTAC Slipper U VS-10U driver.
 * Slipper U is a PC Card to USB converter for data communication card
 * adapter.  It supports DDI Pocket's Air H" C@rd, C@rd H" 64, NTT's P-in,
 * P-in m@ater and various data communication card adapters.
 */

#include "opt_uvscom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/serial.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

SYSCTL_NODE(_hw_usb, OID_AUTO, uvscom, CTLFLAG_RW, 0, "USB uvscom");
#ifdef USB_DEBUG
static int	uvscomdebug = 0;
SYSCTL_INT(_hw_usb_uvscom, OID_AUTO, debug, CTLFLAG_RW,
	   &uvscomdebug, 0, "uvscom debug level");

#define DPRINTFN(n, x) do { \
				if (uvscomdebug > (n)) \
					printf x; \
			} while (0)
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define UVSCOM_MODVER		1	/* module version */

#define	UVSCOM_CONFIG_INDEX	0
#define	UVSCOM_IFACE_INDEX	0

#ifndef UVSCOM_INTR_INTERVAL
#define UVSCOM_INTR_INTERVAL	100	/* mS */
#endif

#define UVSCOM_UNIT_WAIT	5

/* Request */
#define UVSCOM_SET_SPEED	0x10
#define UVSCOM_LINE_CTL		0x11
#define UVSCOM_SET_PARAM	0x12
#define UVSCOM_READ_STATUS	0xd0
#define UVSCOM_SHUTDOWN		0xe0

/* UVSCOM_SET_SPEED parameters */
#define UVSCOM_SPEED_150BPS	0x00
#define UVSCOM_SPEED_300BPS	0x01
#define UVSCOM_SPEED_600BPS	0x02
#define UVSCOM_SPEED_1200BPS	0x03
#define UVSCOM_SPEED_2400BPS	0x04
#define UVSCOM_SPEED_4800BPS	0x05
#define UVSCOM_SPEED_9600BPS	0x06
#define UVSCOM_SPEED_19200BPS	0x07
#define UVSCOM_SPEED_38400BPS	0x08
#define UVSCOM_SPEED_57600BPS	0x09
#define UVSCOM_SPEED_115200BPS	0x0a

/* UVSCOM_LINE_CTL parameters */
#define UVSCOM_BREAK		0x40
#define UVSCOM_RTS		0x02
#define UVSCOM_DTR		0x01
#define UVSCOM_LINE_INIT	0x08

/* UVSCOM_SET_PARAM parameters */
#define UVSCOM_DATA_MASK	0x03
#define UVSCOM_DATA_BIT_8	0x03
#define UVSCOM_DATA_BIT_7	0x02
#define UVSCOM_DATA_BIT_6	0x01
#define UVSCOM_DATA_BIT_5	0x00

#define UVSCOM_STOP_MASK	0x04
#define UVSCOM_STOP_BIT_2	0x04
#define UVSCOM_STOP_BIT_1	0x00

#define UVSCOM_PARITY_MASK	0x18
#define UVSCOM_PARITY_EVEN	0x18
#if 0
#define UVSCOM_PARITY_UNK	0x10
#endif
#define UVSCOM_PARITY_ODD	0x08
#define UVSCOM_PARITY_NONE	0x00

/* Status bits */
#define UVSCOM_TXRDY		0x04
#define UVSCOM_RXRDY		0x01

#define UVSCOM_DCD		0x08
#define UVSCOM_NOCARD		0x04
#define UVSCOM_DSR		0x02
#define UVSCOM_CTS		0x01
#define UVSCOM_USTAT_MASK	(UVSCOM_NOCARD | UVSCOM_DSR | UVSCOM_CTS)

struct	uvscom_softc {
	struct ucom_softc	sc_ucom;

	int			sc_iface_number;/* interface number */

	usbd_interface_handle	sc_intr_iface;	/* interrupt interface */
	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
	int			sc_isize;

	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */

	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* uvscom status register */

	uint16_t		sc_lcr;		/* Line control */
	u_char			sc_usr;		/* unit status */

	struct task		sc_task;
};

/*
 * These are the maximum number of bytes transferred per frame.
 * The output buffer size cannot be increased due to the size encoding.
 */
#define UVSCOMIBUFSIZE		512
#define UVSCOMOBUFSIZE		64

#ifndef UVSCOM_DEFAULT_OPKTSIZE
#define UVSCOM_DEFAULT_OPKTSIZE	8
#endif

static	usbd_status uvscom_shutdown(struct uvscom_softc *);
static	usbd_status uvscom_reset(struct uvscom_softc *);
static	usbd_status uvscom_set_line_coding(struct uvscom_softc *,
					   uint16_t, uint16_t);
static	usbd_status uvscom_set_line(struct uvscom_softc *, uint16_t);
static	usbd_status uvscom_set_crtscts(struct uvscom_softc *);
static	void uvscom_get_status(void *, int, u_char *, u_char *);
static	void uvscom_dtr(struct uvscom_softc *, int);
static	void uvscom_rts(struct uvscom_softc *, int);
static	void uvscom_break(struct uvscom_softc *, int);

static	void uvscom_set(void *, int, int, int);
static	void uvscom_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
#if 0 /* TODO */
static	int  uvscom_ioctl(void *, int, u_long, caddr_t, int, usb_proc_ptr);
#endif
static	int  uvscom_param(void *, int, struct termios *);
static	int  uvscom_open(void *, int);
static	void uvscom_close(void *, int);
static	void uvscom_notify(void *, int);

struct ucom_callback uvscom_callback = {
	uvscom_get_status,
	uvscom_set,
	uvscom_param,
	NULL, /* uvscom_ioctl, TODO */
	uvscom_open,
	uvscom_close,
	NULL,
	NULL
};

static const struct usb_devno uvscom_devs [] = {
	/* SUNTAC U-Cable type A4 */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_AS144L4 },
	/* SUNTAC U-Cable type D2 */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_DS96L },
	/* SUNTAC Ir-Trinity */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_IS96U },
	/* SUNTAC U-Cable type P1 */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_PS64P1 },
	/* SUNTAC Slipper U */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_VS10U },
};
#define uvscom_lookup(v, p) usb_lookup(uvscom_devs, v, p)

static device_probe_t uvscom_match;
static device_attach_t uvscom_attach;
static device_detach_t uvscom_detach;

static device_method_t uvscom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uvscom_match),
	DEVMETHOD(device_attach, uvscom_attach),
	DEVMETHOD(device_detach, uvscom_detach),
	{ 0, 0 }
};

static driver_t uvscom_driver = {
	"ucom",
	uvscom_methods,
	sizeof (struct uvscom_softc)
};

DRIVER_MODULE(uvscom, uhub, uvscom_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uvscom, usb, 1, 1, 1);
MODULE_DEPEND(uvscom, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(uvscom, UVSCOM_MODVER);

static int	uvscomobufsiz = UVSCOM_DEFAULT_OPKTSIZE;
static int	uvscominterval = UVSCOM_INTR_INTERVAL;

static int
sysctl_hw_usb_uvscom_opktsize(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = uvscomobufsiz;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (0 < val && val <= UVSCOMOBUFSIZE)
		uvscomobufsiz = val;
	else
		err = EINVAL;

	return (err);
}

static int
sysctl_hw_usb_uvscom_interval(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = uvscominterval;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (0 < val && val <= 1000)
		uvscominterval = val;
	else
		err = EINVAL;

	return (err);
}

SYSCTL_PROC(_hw_usb_uvscom, OID_AUTO, opktsize, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(int), sysctl_hw_usb_uvscom_opktsize,
	    "I", "uvscom output packet size");
SYSCTL_PROC(_hw_usb_uvscom, OID_AUTO, interval, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(int), sysctl_hw_usb_uvscom_interval,
	    "I", "uvscom interrpt pipe interval");

static int
uvscom_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (uvscom_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

static int
uvscom_attach(device_t self)
{
	struct uvscom_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	struct ucom_softc *ucom;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status err;
	int i;

	ucom = &sc->sc_ucom;
	ucom->sc_dev = self;
	ucom->sc_udev = dev;
	ucom->sc_iface = uaa->iface;

	DPRINTF(("uvscom attach: sc = %p\n", sc));

	/* initialize endpoints */
	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UVSCOM_CONFIG_INDEX, 1);
	if (err) {
		device_printf(self, "failed to set configuration, err=%s\n",
		    usbd_errstr(err));
		goto error;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(ucom->sc_udev);

	if (cdesc == NULL) {
		device_printf(self, "failed to get configuration descriptor\n");
		goto error;
	}

	/* get the common interface */
	err = usbd_device2interface_handle(dev, UVSCOM_IFACE_INDEX,
					   &ucom->sc_iface);
	if (err) {
		device_printf(self, "failed to get interface, err=%s\n",
		    usbd_errstr(err));
		goto error;
	}

	id = usbd_get_interface_descriptor(ucom->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	/* Find endpoints */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			device_printf(self, "no endpoint descriptor for %d\n",
			    i);
			goto error;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkout_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
		}
	}

	if (ucom->sc_bulkin_no == -1) {
		device_printf(self, "Could not find data bulk in\n");
		goto error;
	}
	if (ucom->sc_bulkout_no == -1) {
		device_printf(self, "Could not find data bulk out\n");
		goto error;
	}
	if (sc->sc_intr_number == -1) {
		device_printf(self, "Could not find interrupt in\n");
		goto error;
	}

	sc->sc_dtr = sc->sc_rts = 0;
	sc->sc_lcr = UVSCOM_LINE_INIT;

	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = UVSCOMIBUFSIZE;
	ucom->sc_obufsize = uvscomobufsiz;
	ucom->sc_ibufsizepad = UVSCOMIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uvscom_callback;

	err = uvscom_reset(sc);

	if (err) {
		device_printf(self, "reset failed, %s\n", usbd_errstr(err));
		goto error;
	}

	DPRINTF(("uvscom: in = 0x%x out = 0x%x intr = 0x%x\n",
		 ucom->sc_bulkin_no, ucom->sc_bulkout_no, sc->sc_intr_number));

	TASK_INIT(&sc->sc_task, 0, uvscom_notify, sc);
	ucom_attach(&sc->sc_ucom);
	return 0;

error:
	ucom->sc_dying = 1;
	return ENXIO;
}

static int
uvscom_detach(device_t self)
{
	struct uvscom_softc *sc = device_get_softc(self);
	int rv = 0;

	DPRINTF(("uvscom_detach: sc = %p\n", sc));

	sc->sc_ucom.sc_dying = 1;

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}

	rv = ucom_detach(&sc->sc_ucom);

	return (rv);
}

static usbd_status
uvscom_readstat(struct uvscom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;
	uint16_t r;

	DPRINTF(("%s: send readstat\n", device_get_nameunit(sc->sc_ucom.sc_dev)));

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UVSCOM_READ_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, &r);
	if (err) {
		device_printf(sc->sc_ucom.sc_dev, "uvscom_readstat: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	DPRINTF(("%s: uvscom_readstat: r = %d\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), r));

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
uvscom_shutdown(struct uvscom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: send shutdown\n", device_get_nameunit(sc->sc_ucom.sc_dev)));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_SHUTDOWN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, NULL);
	if (err) {
		device_printf(sc->sc_ucom.sc_dev, "uvscom_shutdown: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
uvscom_reset(struct uvscom_softc *sc)
{
	DPRINTF(("%s: uvscom_reset\n", device_get_nameunit(sc->sc_ucom.sc_dev)));

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
uvscom_set_crtscts(struct uvscom_softc *sc)
{
	DPRINTF(("%s: uvscom_set_crtscts\n", device_get_nameunit(sc->sc_ucom.sc_dev)));

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
uvscom_set_line(struct uvscom_softc *sc, uint16_t line)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: uvscom_set_line: %04x\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), line));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_LINE_CTL;
	USETW(req.wValue, line);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, NULL);
	if (err) {
		device_printf(sc->sc_ucom.sc_dev, "uvscom_set_line: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
uvscom_set_line_coding(struct uvscom_softc *sc, uint16_t lsp, uint16_t ls)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: uvscom_set_line_coding: %02x %02x\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), lsp, ls));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_SET_SPEED;
	USETW(req.wValue, lsp);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, NULL);
	if (err) {
		device_printf(sc->sc_ucom.sc_dev, "uvscom_set_line_coding: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_SET_PARAM;
	USETW(req.wValue, ls);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, NULL);
	if (err) {
		device_printf(sc->sc_ucom.sc_dev, "uvscom_set_line_coding: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

static void
uvscom_dtr(struct uvscom_softc *sc, int onoff)
{
	DPRINTF(("%s: uvscom_dtr: onoff = %d\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), onoff));

	if (sc->sc_dtr == onoff)
		return;			/* no change */

	sc->sc_dtr = onoff;

	if (onoff)
		SET(sc->sc_lcr, UVSCOM_DTR);
	else
		CLR(sc->sc_lcr, UVSCOM_DTR);

	uvscom_set_line(sc, sc->sc_lcr);
}

static void
uvscom_rts(struct uvscom_softc *sc, int onoff)
{
	DPRINTF(("%s: uvscom_rts: onoff = %d\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), onoff));

	if (sc->sc_rts == onoff)
		return;			/* no change */

	sc->sc_rts = onoff;

	if (onoff)
		SET(sc->sc_lcr, UVSCOM_RTS);
	else
		CLR(sc->sc_lcr, UVSCOM_RTS);

	uvscom_set_line(sc, sc->sc_lcr);
}

static void
uvscom_break(struct uvscom_softc *sc, int onoff)
{
	DPRINTF(("%s: uvscom_break: onoff = %d\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), onoff));

	if (onoff)
		uvscom_set_line(sc, SET(sc->sc_lcr, UVSCOM_BREAK));
}

static void
uvscom_set(void *addr, int portno, int reg, int onoff)
{
	struct uvscom_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		uvscom_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		uvscom_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		uvscom_break(sc, onoff);
		break;
	default:
		break;
	}
}

static int
uvscom_param(void *addr, int portno, struct termios *t)
{
	struct uvscom_softc *sc = addr;
	usbd_status err;
	uint16_t lsp;
	uint16_t ls;

	DPRINTF(("%s: uvscom_param: sc = %p\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), sc));

	ls = 0;

	switch (t->c_ospeed) {
	case B150:
		lsp = UVSCOM_SPEED_150BPS;
		break;
	case B300:
		lsp = UVSCOM_SPEED_300BPS;
		break;
	case B600:
		lsp = UVSCOM_SPEED_600BPS;
		break;
	case B1200:
		lsp = UVSCOM_SPEED_1200BPS;
		break;
	case B2400:
		lsp = UVSCOM_SPEED_2400BPS;
		break;
	case B4800:
		lsp = UVSCOM_SPEED_4800BPS;
		break;
	case B9600:
		lsp = UVSCOM_SPEED_9600BPS;
		break;
	case B19200:
		lsp = UVSCOM_SPEED_19200BPS;
		break;
	case B38400:
		lsp = UVSCOM_SPEED_38400BPS;
		break;
	case B57600:
		lsp = UVSCOM_SPEED_57600BPS;
		break;
	case B115200:
		lsp = UVSCOM_SPEED_115200BPS;
		break;
	default:
		return (EIO);
	}

	if (ISSET(t->c_cflag, CSTOPB))
		SET(ls, UVSCOM_STOP_BIT_2);
	else
		SET(ls, UVSCOM_STOP_BIT_1);

	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			SET(ls, UVSCOM_PARITY_ODD);
		else
			SET(ls, UVSCOM_PARITY_EVEN);
	} else
		SET(ls, UVSCOM_PARITY_NONE);

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		SET(ls, UVSCOM_DATA_BIT_5);
		break;
	case CS6:
		SET(ls, UVSCOM_DATA_BIT_6);
		break;
	case CS7:
		SET(ls, UVSCOM_DATA_BIT_7);
		break;
	case CS8:
		SET(ls, UVSCOM_DATA_BIT_8);
		break;
	default:
		return (EIO);
	}

	err = uvscom_set_line_coding(sc, lsp, ls);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CRTSCTS)) {
		err = uvscom_set_crtscts(sc);
		if (err)
			return (EIO);
	}

	return (0);
}

static int
uvscom_open(void *addr, int portno)
{
	struct uvscom_softc *sc = addr;
	int err;
	int i;

	if (sc->sc_ucom.sc_dying)
		return (ENXIO);

	DPRINTF(("uvscom_open: sc = %p\n", sc));

	/* change output packet size */
	sc->sc_ucom.sc_obufsize = uvscomobufsiz;

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		DPRINTF(("uvscom_open: open interrupt pipe.\n"));

		sc->sc_usr = 0;		/* clear unit status */

		err = uvscom_readstat(sc);
		if (err) {
			DPRINTF(("%s: uvscom_open: readstat faild\n",
				 device_get_nameunit(sc->sc_ucom.sc_dev)));
			return (ENXIO);
		}

		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_ucom.sc_iface,
					  sc->sc_intr_number,
					  USBD_SHORT_XFER_OK,
					  &sc->sc_intr_pipe,
					  sc,
					  sc->sc_intr_buf,
					  sc->sc_isize,
					  uvscom_intr,
					  uvscominterval);
		if (err) {
			device_printf(sc->sc_ucom.sc_dev,
			    "cannot open interrupt pipe (addr %d)\n",
			    sc->sc_intr_number);
			return (ENXIO);
		}
	} else {
		DPRINTF(("uvscom_open: did not open interrupt pipe.\n"));
	}

	if ((sc->sc_usr & UVSCOM_USTAT_MASK) == 0) {
		/* unit is not ready */

		for (i = UVSCOM_UNIT_WAIT; i > 0; --i) {
			pause("uvsop", hz);	/* XXX */
			if (ISSET(sc->sc_usr, UVSCOM_USTAT_MASK))
				break;
		}
		if (i == 0) {
			DPRINTF(("%s: unit is not ready\n",
				 device_get_nameunit(sc->sc_ucom.sc_dev)));
			return (ENXIO);
		}

		/* check PC Card was inserted */
		if (ISSET(sc->sc_usr, UVSCOM_NOCARD)) {
			DPRINTF(("%s: no card\n",
				 device_get_nameunit(sc->sc_ucom.sc_dev)));
			return (ENXIO);
		}
	}

	return (0);
}

static void
uvscom_close(void *addr, int portno)
{
	struct uvscom_softc *sc = addr;
	int err;

	if (sc->sc_ucom.sc_dying)
		return;

	DPRINTF(("uvscom_close: close\n"));

	uvscom_shutdown(sc);

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			device_printf(sc->sc_ucom.sc_dev,
			    "abort interrupt pipe failed: %s\n",
			    usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			device_printf(sc->sc_ucom.sc_dev,
			    "close interrupt pipe failed: %s\n",
			    usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}

static void
uvscom_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct uvscom_softc *sc = priv;
	u_char *buf = sc->sc_intr_buf;
	u_char pstatus;

	if (sc->sc_ucom.sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		device_printf(sc->sc_ucom.sc_dev,	
		    "uvscom_intr: abnormal status: %s\n",
		    usbd_errstr(status));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	DPRINTFN(2, ("%s: uvscom status = %02x %02x\n",
		 device_get_nameunit(sc->sc_ucom.sc_dev), buf[0], buf[1]));

	sc->sc_lsr = sc->sc_msr = 0;
	sc->sc_usr = buf[1];

	pstatus = buf[0];
	if (ISSET(pstatus, UVSCOM_TXRDY))
		SET(sc->sc_lsr, ULSR_TXRDY);
	if (ISSET(pstatus, UVSCOM_RXRDY))
		SET(sc->sc_lsr, ULSR_RXRDY);

	pstatus = buf[1];
	if (ISSET(pstatus, UVSCOM_CTS))
		SET(sc->sc_msr, SER_CTS);
	if (ISSET(pstatus, UVSCOM_DSR))
		SET(sc->sc_msr, SER_DSR);
	if (ISSET(pstatus, UVSCOM_DCD))
		SET(sc->sc_msr, SER_DCD);

	/* Deferred notifying to the ucom layer */
	taskqueue_enqueue(taskqueue_swi_giant, &sc->sc_task);
}

static void
uvscom_notify(void *arg, int count)
{
	struct uvscom_softc *sc;

	sc = (struct uvscom_softc *)arg;
	if (sc->sc_ucom.sc_dying)
		return;
	ucom_status_change(&sc->sc_ucom);
}

static void
uvscom_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct uvscom_softc *sc = addr;

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

#if 0 /* TODO */
static int
uvscom_ioctl(void *addr, int portno, u_long cmd, caddr_t data, int flag,
	     struct thread *p)
{
	struct uvscom_softc *sc = addr;
	int error = 0;

	if (sc->sc_ucom.sc_dying)
		return (EIO);

	DPRINTF(("uvscom_ioctl: cmd = 0x%08lx\n", cmd));

	switch (cmd) {
	case TIOCNOTTY:
	case TIOCMGET:
	case TIOCMSET:
		break;

	default:
		DPRINTF(("uvscom_ioctl: unknown\n"));
		error = ENOTTY;
		break;
	}

	return (error);
}
#endif
