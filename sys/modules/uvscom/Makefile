# $FreeBSD: release/7.0.0/sys/modules/uvscom/Makefile 139399 2004-12-29 08:50:35Z imp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
