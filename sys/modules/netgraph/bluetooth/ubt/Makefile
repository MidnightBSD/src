# $Id: Makefile,v 1.1.1.4 2012-07-21 15:16:45 laffer1 Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

KMOD=		ng_ubt
SRCS=		ng_ubt.c opt_bus.h opt_usb.h device_if.h bus_if.h \
		usb_if.h usbdevs.h

.include <bsd.kmod.mk>
