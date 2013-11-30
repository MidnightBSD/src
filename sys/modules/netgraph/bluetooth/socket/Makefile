# $Id: Makefile,v 1.1.1.2 2006-02-25 02:37:24 laffer1 Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/socket/Makefile,v 1.5 2004/01/13 11:28:49 ru Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/socket

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_btsocket
SRCS=		ng_btsocket.c \
		ng_btsocket_hci_raw.c \
		ng_btsocket_l2cap_raw.c \
		ng_btsocket_l2cap.c \
		ng_btsocket_rfcomm.c

.include <bsd.kmod.mk>
