# $Id: Makefile,v 1.1.1.3 2008-11-27 13:51:53 laffer1 Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bluetooth/Makefile,v 1.6 2006/02/18 23:49:27 yar Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c

.include <bsd.kmod.mk>
