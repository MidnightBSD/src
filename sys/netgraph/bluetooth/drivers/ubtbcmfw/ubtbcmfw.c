/*
 * ubtbcmfw.c
 */

/*-
 * Copyright (c) 2003 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ubtbcmfw.c,v 1.1.1.2 2006-02-25 02:37:33 laffer1 Exp $
 * $FreeBSD: src/sys/netgraph/bluetooth/drivers/ubtbcmfw/ubtbcmfw.c,v 1.10 2005/01/07 01:45:42 imp Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include "usbdevs.h"

/*
 * Download firmware to BCM2033.
 */

#define UBTBCMFW_CONFIG_NO	1	/* Config number */
#define UBTBCMFW_IFACE_IDX	0 	/* Control interface */
#define UBTBCMFW_INTR_IN_EP	0x81	/* Fixed endpoint */
#define UBTBCMFW_BULK_OUT_EP	0x02	/* Fixed endpoint */
#define UBTBCMFW_INTR_IN	UE_GET_ADDR(UBTBCMFW_INTR_IN_EP)
#define UBTBCMFW_BULK_OUT	UE_GET_ADDR(UBTBCMFW_BULK_OUT_EP)

struct ubtbcmfw_softc {
	USBBASEDEVICE		sc_dev;			/* base device */
	usbd_device_handle	sc_udev;		/* USB device handle */
	struct cdev *sc_ctrl_dev;		/* control device */
	struct cdev *sc_intr_in_dev;		/* interrupt device */
	struct cdev *sc_bulk_out_dev;	/* bulk device */
	usbd_pipe_handle	sc_intr_in_pipe;	/* interrupt pipe */
	usbd_pipe_handle	sc_bulk_out_pipe;	/* bulk out pipe */
	int			sc_flags;
#define UBTBCMFW_CTRL_DEV	(1 << 0)
#define UBTBCMFW_INTR_IN_DEV	(1 << 1)
#define UBTBCMFW_BULK_OUT_DEV	(1 << 2)
	int			sc_refcnt;
	int			sc_dying;
};

typedef struct ubtbcmfw_softc	*ubtbcmfw_softc_p;

/*
 * Device methods
 */

#define UBTBCMFW_UNIT(n)	((minor(n) >> 4) & 0xf)
#define UBTBCMFW_ENDPOINT(n)	(minor(n) & 0xf)
#define UBTBCMFW_MINOR(u, e)	(((u) << 4) | (e))
#define UBTBCMFW_BSIZE		1024

Static d_open_t		ubtbcmfw_open;
Static d_close_t	ubtbcmfw_close;
Static d_read_t		ubtbcmfw_read;
Static d_write_t	ubtbcmfw_write;
Static d_ioctl_t	ubtbcmfw_ioctl;
Static d_poll_t		ubtbcmfw_poll;

Static struct cdevsw	ubtbcmfw_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ubtbcmfw_open,
	.d_close =	ubtbcmfw_close,
	.d_read =	ubtbcmfw_read,
	.d_write =	ubtbcmfw_write,
	.d_ioctl =	ubtbcmfw_ioctl,
	.d_poll =	ubtbcmfw_poll,
	.d_name =	"ubtbcmfw",
};

/*
 * Module
 */

USB_DECLARE_DRIVER(ubtbcmfw);
DRIVER_MODULE(ubtbcmfw, uhub, ubtbcmfw_driver, ubtbcmfw_devclass,
	      usbd_driver_load, 0);

/*
 * Probe for a USB Bluetooth device
 */

USB_MATCH(ubtbcmfw)
{
#define	USB_PRODUCT_BROADCOM_BCM2033NF	0x2033

	USB_MATCH_START(ubtbcmfw, uaa);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	/* Match the boot device. */
	if (uaa->vendor == USB_VENDOR_BROADCOM &&
	    uaa->product == USB_PRODUCT_BROADCOM_BCM2033NF)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

/*
 * Attach the device
 */

USB_ATTACH(ubtbcmfw)
{
	USB_ATTACH_START(ubtbcmfw, sc, uaa);
	usbd_interface_handle	iface;
	usbd_status		err;
	char			devinfo[1024];

	sc->sc_udev = uaa->device;
	usbd_devinfo(sc->sc_udev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	sc->sc_ctrl_dev = sc->sc_intr_in_dev = sc->sc_bulk_out_dev = NULL;
	sc->sc_intr_in_pipe = sc->sc_bulk_out_pipe = NULL;
	sc->sc_flags = sc->sc_refcnt = sc->sc_dying = 0;

	err = usbd_set_config_no(sc->sc_udev, UBTBCMFW_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed. %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		goto bad;
	}

	err = usbd_device2interface_handle(sc->sc_udev, UBTBCMFW_IFACE_IDX,
			&iface);
	if (err) {
		printf("%s: getting interface handle failed. %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		goto bad;
	}

	/* Will be used as a bulk pipe */
	err = usbd_open_pipe(iface, UBTBCMFW_INTR_IN_EP, 0,
			&sc->sc_intr_in_pipe);
	if (err) {
		printf("%s: open intr in failed. %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		goto bad;
	}

	err = usbd_open_pipe(iface, UBTBCMFW_BULK_OUT_EP, 0,
			&sc->sc_bulk_out_pipe);
	if (err) {
		printf("%s: open bulk out failed. %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		goto bad;
	}

	/* Create device nodes */
	sc->sc_ctrl_dev = make_dev(&ubtbcmfw_cdevsw,
		UBTBCMFW_MINOR(USBDEVUNIT(sc->sc_dev), 0),
		UID_ROOT, GID_OPERATOR, 0644,
		"%s", USBDEVNAME(sc->sc_dev));

	sc->sc_intr_in_dev = make_dev(&ubtbcmfw_cdevsw,
		UBTBCMFW_MINOR(USBDEVUNIT(sc->sc_dev), UBTBCMFW_INTR_IN),
		UID_ROOT, GID_OPERATOR, 0644,
		"%s.%d", USBDEVNAME(sc->sc_dev), UBTBCMFW_INTR_IN);

	sc->sc_bulk_out_dev = make_dev(&ubtbcmfw_cdevsw,
		UBTBCMFW_MINOR(USBDEVUNIT(sc->sc_dev), UBTBCMFW_BULK_OUT),
		UID_ROOT, GID_OPERATOR, 0644,
		"%s.%d", USBDEVNAME(sc->sc_dev), UBTBCMFW_BULK_OUT);

	USB_ATTACH_SUCCESS_RETURN;
bad:
	ubtbcmfw_detach(self);  
        
        USB_ATTACH_ERROR_RETURN;
}

/*
 * Detach the device
 */

USB_DETACH(ubtbcmfw)
{
	USB_DETACH_START(ubtbcmfw, sc);

	sc->sc_dying = 1;

	if (-- sc->sc_refcnt >= 0) {
		if (sc->sc_intr_in_pipe != NULL) 
			usbd_abort_pipe(sc->sc_intr_in_pipe);

		if (sc->sc_bulk_out_pipe != NULL) 
			usbd_abort_pipe(sc->sc_bulk_out_pipe);

		usb_detach_wait(USBDEV(sc->sc_dev));
	}

	/* Destroy device nodes */
	if (sc->sc_bulk_out_dev != NULL) {
		destroy_dev(sc->sc_bulk_out_dev);
		sc->sc_bulk_out_dev = NULL;
	}

	if (sc->sc_intr_in_dev != NULL) {
		destroy_dev(sc->sc_intr_in_dev);
		sc->sc_intr_in_dev = NULL;
	}

	if (sc->sc_ctrl_dev != NULL) {
		destroy_dev(sc->sc_ctrl_dev);
		sc->sc_ctrl_dev = NULL;
	}

	/* Close pipes */
	if (sc->sc_intr_in_pipe != NULL) {
		usbd_close_pipe(sc->sc_intr_in_pipe);
		sc->sc_intr_in_pipe = NULL;
	}

	if (sc->sc_bulk_out_pipe != NULL) {
		usbd_close_pipe(sc->sc_bulk_out_pipe);
		sc->sc_intr_in_pipe = NULL;
	}

	return (0);
}

/*
 * Open endpoint device
 * XXX FIXME softc locking
 */

Static int
ubtbcmfw_open(struct cdev *dev, int flag, int mode, usb_proc_ptr p)
{
	ubtbcmfw_softc_p	sc = NULL;
	int			error = 0;

	/* checks for sc != NULL */
	USB_GET_SC_OPEN(ubtbcmfw, UBTBCMFW_UNIT(dev), sc);
	if (sc->sc_dying)
		return (ENXIO);

	switch (UBTBCMFW_ENDPOINT(dev)) {
	case USB_CONTROL_ENDPOINT:
		if (!(sc->sc_flags & UBTBCMFW_CTRL_DEV))
			sc->sc_flags |= UBTBCMFW_CTRL_DEV;
		else
			error = EBUSY;
		break;

	case UBTBCMFW_INTR_IN:
		if (!(sc->sc_flags & UBTBCMFW_INTR_IN_DEV)) {
			if (sc->sc_intr_in_pipe != NULL)
				sc->sc_flags |= UBTBCMFW_INTR_IN_DEV;
			else
				error = ENXIO;
		} else
			error = EBUSY;
		break;

	case UBTBCMFW_BULK_OUT:
		if (!(sc->sc_flags & UBTBCMFW_BULK_OUT_DEV)) {
			if (sc->sc_bulk_out_pipe != NULL)
				sc->sc_flags |= UBTBCMFW_BULK_OUT_DEV;
			else
				error = ENXIO;
		} else
			error = EBUSY;
		break;

	default:
		error = ENXIO;
		break;
	}

	return (error);
}

/*
 * Close endpoint device
 * XXX FIXME softc locking
 */

Static int
ubtbcmfw_close(struct cdev *dev, int flag, int mode, usb_proc_ptr p)
{
	ubtbcmfw_softc_p	sc = NULL;

	USB_GET_SC(ubtbcmfw, UBTBCMFW_UNIT(dev), sc);
	if (sc == NULL)
		return (ENXIO);

	switch (UBTBCMFW_ENDPOINT(dev)) {
	case USB_CONTROL_ENDPOINT:
		sc->sc_flags &= ~UBTBCMFW_CTRL_DEV;
		break;

	case UBTBCMFW_INTR_IN:
		if (sc->sc_intr_in_pipe != NULL)
			usbd_abort_pipe(sc->sc_intr_in_pipe);

		sc->sc_flags &= ~UBTBCMFW_INTR_IN_DEV;
		break;

	case UBTBCMFW_BULK_OUT:
		if (sc->sc_bulk_out_pipe != NULL)
			usbd_abort_pipe(sc->sc_bulk_out_pipe);

		sc->sc_flags &= ~UBTBCMFW_BULK_OUT_DEV;
		break;
	}

	return (0);
}

/*
 * Read from the endpoint device
 * XXX FIXME softc locking
 */

Static int
ubtbcmfw_read(struct cdev *dev, struct uio *uio, int flag)
{
	ubtbcmfw_softc_p	sc = NULL;
	u_int8_t		buf[UBTBCMFW_BSIZE];
	usbd_xfer_handle	xfer;
	usbd_status		err;
	int			n, tn, error = 0;

	USB_GET_SC(ubtbcmfw, UBTBCMFW_UNIT(dev), sc);
	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (UBTBCMFW_ENDPOINT(dev) != UBTBCMFW_INTR_IN)
		return (EOPNOTSUPP);
	if (sc->sc_intr_in_pipe == NULL)
		return (ENXIO);

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return (ENOMEM);

	sc->sc_refcnt ++;

	while ((n = min(sizeof(buf), uio->uio_resid)) != 0) {
		tn = n;
		err = usbd_bulk_transfer(xfer, sc->sc_intr_in_pipe,
				USBD_SHORT_XFER_OK, USBD_DEFAULT_TIMEOUT,
				buf, &tn, "bcmrd");
		switch (err) {
		case USBD_NORMAL_COMPLETION:
			error = uiomove(buf, tn, uio);
			break;

		case USBD_INTERRUPTED:
			error = EINTR;
			break;

		case USBD_TIMEOUT:
			error = ETIMEDOUT;
			break;

		default:
			error = EIO;
			break;
		}

		if (error != 0 || tn < n)
			break;
	}

	usbd_free_xfer(xfer);

	if (-- sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));

	return (error);
}

/*
 * Write into the endpoint device
 * XXX FIXME softc locking
 */

Static int
ubtbcmfw_write(struct cdev *dev, struct uio *uio, int flag)
{
	ubtbcmfw_softc_p	sc = NULL;
	u_int8_t		buf[UBTBCMFW_BSIZE];
	usbd_xfer_handle	xfer;
	usbd_status		err;
	int			n, error = 0;

	USB_GET_SC(ubtbcmfw, UBTBCMFW_UNIT(dev), sc);
	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (UBTBCMFW_ENDPOINT(dev) != UBTBCMFW_BULK_OUT)
		return (EOPNOTSUPP);
	if (sc->sc_bulk_out_pipe == NULL)
		return (ENXIO);

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return (ENOMEM);

	sc->sc_refcnt ++;

	while ((n = min(sizeof(buf), uio->uio_resid)) != 0) {
		error = uiomove(buf, n, uio);
		if (error != 0)
			break;

		err = usbd_bulk_transfer(xfer, sc->sc_bulk_out_pipe,
				0, USBD_DEFAULT_TIMEOUT, buf, &n, "bcmwr");
		switch (err) {
		case USBD_NORMAL_COMPLETION:
			break;

		case USBD_INTERRUPTED:
			error = EINTR;
			break;

		case USBD_TIMEOUT:
			error = ETIMEDOUT;
			break;

		default:
			error = EIO;
			break;
		}

		if (error != 0)
			break;
	}

	usbd_free_xfer(xfer);

	if (-- sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));

	return (error);
}

/*
 * Process ioctl on the endpoint device
 * XXX FIXME softc locking
 */

Static int
ubtbcmfw_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	ubtbcmfw_softc_p	sc = NULL;
	int			error = 0;

	USB_GET_SC(ubtbcmfw, UBTBCMFW_UNIT(dev), sc);
	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (UBTBCMFW_ENDPOINT(dev) != USB_CONTROL_ENDPOINT)
		return (EOPNOTSUPP);

	sc->sc_refcnt ++;

	switch (cmd) {
	case USB_GET_DEVICE_DESC:
		*(usb_device_descriptor_t *) data =
				*usbd_get_device_descriptor(sc->sc_udev);
		break;

	default:
		error = EINVAL;
		break;
	}

	if (-- sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));

	return (error);
}

/*
 * Poll the endpoint device
 * XXX FIXME softc locking
 */

Static int
ubtbcmfw_poll(struct cdev *dev, int events, usb_proc_ptr p)
{
	ubtbcmfw_softc_p	sc = NULL;
	int			revents = 0;

	USB_GET_SC(ubtbcmfw, UBTBCMFW_UNIT(dev), sc);
	if (sc == NULL)
		return (ENXIO);

	switch (UBTBCMFW_ENDPOINT(dev)) {
	case UBTBCMFW_INTR_IN:
		if (sc->sc_intr_in_pipe != NULL)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			revents = ENXIO;
		break;

	case UBTBCMFW_BULK_OUT:
		if (sc->sc_bulk_out_pipe != NULL)
			revents |= events & (POLLOUT | POLLWRNORM);
		else
			revents = ENXIO;
		break;

	default:
		revents = EOPNOTSUPP;
		break;
	}

	return (revents);
}

