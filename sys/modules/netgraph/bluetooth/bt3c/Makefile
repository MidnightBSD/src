# $Id: Makefile,v 1.2 2002/11/24 20:50:56 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/bt3c/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/drivers/bt3c

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include \
		-I${SRCTOP}/sys/netgraph/bluetooth/drivers/bt3c

KMOD=		ng_bt3c
SRCS=		ng_bt3c_pccard.c bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
