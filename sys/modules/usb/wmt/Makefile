# $FreeBSD: stable/11/sys/modules/usb/wmt/Makefile 322695 2017-08-19 17:00:10Z wulf $

S=     ${SRCTOP}/sys

.PATH: $S/dev/usb/input

KMOD=	wmt
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h usbdevs.h wmt.c

.include <bsd.kmod.mk>
