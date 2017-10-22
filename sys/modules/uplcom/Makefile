# $FreeBSD: release/7.0.0/sys/modules/uplcom/Makefile 139399 2004-12-29 08:50:35Z imp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
