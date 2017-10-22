/*-
 * Copyright (c) 2001-2007 M. Warner Losh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This code is based on ugen.c and ulpt.c developed by Lennart Augustsson.
 * This code includes software developed by the NetBSD Foundation, Inc. and
 * its contributors.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/usb/ufm.c 170969 2007-06-21 14:42:34Z imp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include "usbdevs.h"
#include <dev/usb/dsbr100io.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ufmdebug) printf x
#define DPRINTFN(n,x)	if (ufmdebug>(n)) printf x
int	ufmdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ufm, CTLFLAG_RW, 0, "USB ufm");
SYSCTL_INT(_hw_usb_ufm, OID_AUTO, debug, CTLFLAG_RW,
	   &ufmdebug, 0, "ufm debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

d_open_t  ufmopen;
d_close_t ufmclose;
d_ioctl_t ufmioctl;

static struct cdevsw ufm_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ufmopen,
	.d_close =	ufmclose,
	.d_ioctl =	ufmioctl,
	.d_name =	"ufm",
};

#define FM_CMD0		0x00
#define FM_CMD_SET_FREQ	0x01
#define FM_CMD2		0x02

struct ufm_softc {
 	device_t sc_dev;
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;

	int sc_opened;
	int sc_epaddr;
	int sc_freq;

	int sc_refcnt;
};

#define UFMUNIT(n) (minor(n))

static device_probe_t ufm_match;
static device_attach_t ufm_attach;
static device_detach_t ufm_detach;

static device_method_t ufm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ufm_match),
	DEVMETHOD(device_attach,	ufm_attach),
	DEVMETHOD(device_detach,	ufm_detach),

	{ 0, 0 }
};

static driver_t ufm_driver = {
	"ufm",
	ufm_methods,
	sizeof(struct ufm_softc)
};

static devclass_t ufm_devclass;

static int
ufm_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_device_descriptor_t *dd;

	DPRINTFN(10,("ufm_match\n"));
	if (!uaa->iface)
		return UMATCH_NONE;

	dd = usbd_get_device_descriptor(uaa->device);

	if (dd &&
	    ((UGETW(dd->idVendor) == USB_VENDOR_CYPRESS &&
	    UGETW(dd->idProduct) == USB_PRODUCT_CYPRESS_FMRADIO)))
		return UMATCH_VENDOR_PRODUCT;
	else
		return UMATCH_NONE;
}

static int
ufm_attach(device_t self)
{
	struct ufm_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_endpoint_descriptor_t *edesc;
	usbd_device_handle udev;
	usbd_interface_handle iface;
	u_int8_t epcount;
	usbd_status r;
	char * ermsg = "<none>";

	DPRINTFN(10,("ufm_attach: sc=%p\n", sc));
	sc->sc_dev = self;
	sc->sc_udev = udev = uaa->device;

 	if ((!uaa->device) || (!uaa->iface)) {
		ermsg = "device or iface";
 		goto nobulk;
	}
	sc->sc_iface = iface = uaa->iface;
	sc->sc_opened = 0;
	sc->sc_refcnt = 0;

	r = usbd_endpoint_count(iface, &epcount);
	if (r != USBD_NORMAL_COMPLETION) {
		ermsg = "endpoints";
		goto nobulk;
	}

	edesc = usbd_interface2endpoint_descriptor(iface, 0);
	if (!edesc) {
		ermsg = "interface endpoint";
		goto nobulk;
	}
	sc->sc_epaddr = edesc->bEndpointAddress;

	/* XXX no error trapping, no storing of struct cdev **/
	(void) make_dev(&ufm_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "ufm%d", device_get_unit(self));
	DPRINTFN(10, ("ufm_attach: %p\n", sc->sc_udev));
	return 0;

 nobulk:
	device_printf(sc->sc_dev, "could not find %s\n", ermsg);
	return ENXIO;
}


int
ufmopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct ufm_softc *sc;

	int unit = UFMUNIT(dev);
	sc = devclass_get_softc(ufm_devclass, unit);
	if (sc == NULL)
		return (ENXIO);

	DPRINTFN(5, ("ufmopen: flag=%d, mode=%d, unit=%d\n",
		     flag, mode, unit));

	if (sc->sc_opened)
		return (EBUSY);

	if ((flag & (FWRITE|FREAD)) != (FWRITE|FREAD))
		return (EACCES);

	sc->sc_opened = 1;
	return (0);
}

int
ufmclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct ufm_softc *sc;

	int unit = UFMUNIT(dev);
	sc = devclass_get_softc(ufm_devclass, unit);

	DPRINTFN(5, ("ufmclose: flag=%d, mode=%d, unit=%d\n", flag, mode, unit));
	sc->sc_opened = 0;
	sc->sc_refcnt = 0;
	return 0;
}

static int
ufm_do_req(struct ufm_softc *sc, u_int8_t reqtype, u_int8_t request,
    u_int16_t value, u_int16_t index, u_int8_t len, void *retbuf)
{
	int s;
	usb_device_request_t req;
	usbd_status err;

	s = splusb();
	req.bmRequestType = reqtype;
	req.bRequest = request;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, len);
	err = usbd_do_request_flags(sc->sc_udev, &req, retbuf, 0, NULL,
	    USBD_DEFAULT_TIMEOUT);
	splx(s);
	if (err)
		return (EIO);
	return (0);
}

static int
ufm_set_freq(struct ufm_softc *sc, caddr_t addr)
{
	int freq = *(int *)addr;
	u_int8_t ret;

	/*
	 * Freq now is in Hz.  We need to convert it to the frequency
	 * that the radio wants.  This frequency is 10.7MHz above
	 * the actual frequency.  We then need to convert to
	 * units of 12.5kHz.  We add one to the IFM to make rounding
	 * easier.
	 */
	sc->sc_freq = freq;
	freq = (freq + 10700001) / 12500;
	/* This appears to set the frequency */
	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD_SET_FREQ, freq >> 8,
	    freq, 1, &ret) != 0)
		return (EIO);
	/* Not sure what this does */
	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD0, 0x96, 0xb7, 1,
	    &ret) != 0)
		return (EIO);
	return (0);
}

static int
ufm_get_freq(struct ufm_softc *sc, caddr_t addr)
{
	int *valp = (int *)addr;
	*valp = sc->sc_freq;
	return (0);
}

static int
ufm_start(struct ufm_softc *sc, caddr_t addr)
{
	u_int8_t ret;

	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD0, 0x00, 0xc7,
	    1, &ret))
		return (EIO);
	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD2, 0x01, 0x00,
	    1, &ret))
		return (EIO);
	if (ret & 0x1)
		return (EIO);
	return (0);
}

static int
ufm_stop(struct ufm_softc *sc, caddr_t addr)
{
	u_int8_t ret;

	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD0, 0x16, 0x1C,
	    1, &ret))
		return (EIO);
	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD2, 0x00, 0x00,
	    1, &ret))
		return (EIO);
	return (0);
}

static int
ufm_get_stat(struct ufm_softc *sc, caddr_t addr)
{
	u_int8_t ret;

	/*
	 * Note, there's a 240ms settle time before the status
	 * will be valid, so tsleep that amount.  hz/4 is a good
	 * approximation of that.  Since this is a short sleep
	 * we don't try to catch any signals to keep things
	 * simple.
	 */
	tsleep(sc, 0, "ufmwait", hz/4);
	if (ufm_do_req(sc, UT_READ_VENDOR_DEVICE, FM_CMD0, 0x00, 0x24,
	    1, &ret))
		return (EIO);
	*(int *)addr = ret;

	return (0);
}

int
ufmioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct ufm_softc *sc;

	int unit = UFMUNIT(dev);
	int error = 0;

	sc = devclass_get_softc(ufm_devclass, unit);

	switch (cmd) {
	case FM_SET_FREQ:
		error = ufm_set_freq(sc, addr);
		break;
	case FM_GET_FREQ:
		error = ufm_get_freq(sc, addr);
		break;
	case FM_START:
		error = ufm_start(sc, addr);
		break;
	case FM_STOP:
		error = ufm_stop(sc, addr);
		break;
	case FM_GET_STAT:
		error = ufm_get_stat(sc, addr);
		break;
	default:
		return ENOTTY;
		break;
	}
	return error;
}

static int
ufm_detach(device_t self)
{
	return 0;
}

MODULE_DEPEND(ufm, usb, 1, 1, 1);
DRIVER_MODULE(ufm, uhub, ufm_driver, ufm_devclass, usbd_driver_load, 0);
