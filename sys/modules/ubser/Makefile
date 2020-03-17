# $FreeBSD: stable/11/sys/modules/ubser/Makefile 319182 2017-05-30 04:11:12Z ngie $

S=	${SRCTOP}/sys
.PATH: $S/dev/usb/serial

KMOD=	ubser
SRCS=	ubser.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
