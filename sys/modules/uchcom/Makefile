# $FreeBSD: stable/11/sys/modules/uchcom/Makefile 346521 2019-04-22 04:11:37Z ian $

S=	${SRCTOP}/sys
.PATH: $S/dev/usb/serial

KMOD=	uchcom
SRCS=	uchcom.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
