# $FreeBSD: release/7.0.0/sys/modules/umass/Makefile 146957 2005-06-04 10:58:39Z schweikh $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	umass
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_cam.h opt_scsi.h \
	umass.c usbdevs.h

.include <bsd.kmod.mk>
