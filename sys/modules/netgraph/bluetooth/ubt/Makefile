# $Id: Makefile,v 1.2 2003/03/22 23:44:34 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/ubt/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/drivers/ubt

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include \
		-I${SRCTOP}/sys/netgraph/bluetooth/drivers/ubt

KMOD=		ng_ubt
SRCS=		ng_ubt.c ng_ubt_intel.c opt_bus.h opt_usb.h device_if.h \
		bus_if.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
