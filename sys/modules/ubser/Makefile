# $FreeBSD: release/10.0.0/sys/modules/ubser/Makefile 198041 2009-10-13 18:07:56Z jhb $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb/serial

KMOD=	ubser
SRCS=	ubser.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
