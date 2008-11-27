# $Id: Makefile,v 1.1.1.3 2008-11-27 13:51:53 laffer1 Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/ubt/Makefile,v 1.8 2006/02/18 23:49:27 yar Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

KMOD=		ng_ubt
SRCS=		ng_ubt.c bus_if.h device_if.h opt_usb.h usbdevs.h

.include <bsd.kmod.mk>
