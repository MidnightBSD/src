# $FreeBSD: release/7.0.0/sys/modules/kue/Makefile 150636 2005-09-27 18:10:43Z mlaier $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_kue
SRCS=	if_kue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
