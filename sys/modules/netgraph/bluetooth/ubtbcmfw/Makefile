# $Id: Makefile,v 1.1.1.3 2008-11-27 13:51:53 laffer1 Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/ubtbcmfw/Makefile,v 1.6 2006/02/18 23:49:28 yar Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

KMOD=		ubtbcmfw
SRCS=		ubtbcmfw.c bus_if.h device_if.h opt_usb.h usbdevs.h

.include <bsd.kmod.mk>
