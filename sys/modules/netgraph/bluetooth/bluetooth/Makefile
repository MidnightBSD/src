# $Id: Makefile,v 1.1 2002/11/24 20:39:57 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/bluetooth/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/common

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c

.include <bsd.kmod.mk>
