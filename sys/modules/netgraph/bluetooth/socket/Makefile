# $Id: Makefile,v 1.2 2003/01/12 23:37:31 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/socket/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/socket

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include

KMOD=		ng_btsocket
SRCS=		ng_btsocket.c \
		ng_btsocket_hci_raw.c \
		ng_btsocket_l2cap_raw.c \
		ng_btsocket_l2cap.c \
		ng_btsocket_rfcomm.c \
		ng_btsocket_sco.c

.include <bsd.kmod.mk>
