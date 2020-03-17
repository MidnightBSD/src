# $FreeBSD: stable/11/sys/modules/usb/cfumass/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/usb/storage
KMOD=	cfumass

SRCS=	bus_if.h device_if.h opt_bus.h opt_usb.h usb_if.h usbdevs.h \
	cfumass.c

.include <bsd.kmod.mk>
