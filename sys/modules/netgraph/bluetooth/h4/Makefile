# $Id: Makefile,v 1.1.1.2 2006-02-25 02:37:23 laffer1 Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/h4/Makefile,v 1.5 2004/01/13 11:28:49 ru Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/h4

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/h4

KMOD=		ng_h4
SRCS=		ng_h4.c

.include <bsd.kmod.mk>

