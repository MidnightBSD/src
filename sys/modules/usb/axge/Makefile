# $FreeBSD: stable/11/sys/modules/usb/axge/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/usb/net

KMOD=	if_axge
SRCS+=	if_axge.c usbdevs.h
SRCS+=	bus_if.h device_if.h miibus_if.h usb_if.h \
	opt_bus.h opt_inet.h opt_usb.h

.include <bsd.kmod.mk>
