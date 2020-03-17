# $Id: Makefile,v 1.1 2002/11/24 20:40:04 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/h4/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/drivers/h4

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include \
		-I${SRCTOP}/sys/netgraph/bluetooth/drivers/h4

KMOD=		ng_h4
SRCS=		ng_h4.c

.include <bsd.kmod.mk>
