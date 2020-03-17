# $Id: Makefile,v 1.1 2003/04/27 00:22:12 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/ubtbcmfw/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/drivers/ubtbcmfw

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include \
		-I${SRCTOP}/sys/netgraph/bluetooth/drivers/ubtbcmfw

KMOD=		ubtbcmfw
SRCS=		ubtbcmfw.c opt_bus.h opt_usb.h device_if.h bus_if.h \
		usb_if.h vnode_if.h usbdevs.h

.include <bsd.kmod.mk>
