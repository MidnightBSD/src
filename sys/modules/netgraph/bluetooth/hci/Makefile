# $Id: Makefile,v 1.1 2002/11/24 20:40:05 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/hci/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/hci

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include \
		-I${SRCTOP}/sys/netgraph/bluetooth/hci

KMOD=		ng_hci
SRCS=		ng_hci_main.c ng_hci_cmds.c ng_hci_evnt.c  \
		ng_hci_ulpi.c ng_hci_misc.c

.include <bsd.kmod.mk>
