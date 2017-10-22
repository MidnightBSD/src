# $FreeBSD: release/10.0.0/sys/modules/usb/uep/Makefile 208554 2010-05-25 21:20:56Z glebius $

S=     ${.CURDIR}/../../..

.PATH: $S/dev/usb/input

KMOD=	uep
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h vnode_if.h usbdevs.h \
	uep.c

.include <bsd.kmod.mk>
