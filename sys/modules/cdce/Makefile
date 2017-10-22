# $FreeBSD: release/7.0.0/sys/modules/cdce/Makefile 150636 2005-09-27 18:10:43Z mlaier $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cdce
SRCS=	if_cdce.c opt_usb.h device_if.h bus_if.h
SRCS+=	usbdevs.h

.include <bsd.kmod.mk>
