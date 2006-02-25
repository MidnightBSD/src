# $Id: Makefile,v 1.1.1.2 2006-02-25 02:37:24 laffer1 Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/hci/Makefile,v 1.5 2004/01/13 11:28:49 ru Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/hci

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/hci

KMOD=		ng_hci
SRCS=		ng_hci_main.c ng_hci_cmds.c ng_hci_evnt.c  \
		ng_hci_ulpi.c ng_hci_misc.c

.include <bsd.kmod.mk>
