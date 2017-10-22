# $Id: Makefile,v 1.2 2003/03/22 23:44:34 max Exp $
# $FreeBSD: release/10.0.0/sys/modules/netgraph/bluetooth/ubt/Makefile 192909 2009-05-27 16:43:40Z thompsa $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

KMOD=		ng_ubt
SRCS=		ng_ubt.c opt_bus.h opt_usb.h device_if.h bus_if.h \
		usb_if.h usbdevs.h

.include <bsd.kmod.mk>
