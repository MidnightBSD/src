# $Id: Makefile,v 1.1 2002/11/24 20:40:11 max Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/bluetooth/l2cap/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/l2cap

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include \
		-I${SRCTOP}/sys/netgraph/bluetooth/l2cap

KMOD=		ng_l2cap
SRCS=		ng_l2cap_main.c ng_l2cap_cmds.c ng_l2cap_evnt.c \
		ng_l2cap_ulpi.c ng_l2cap_llpi.c ng_l2cap_misc.c

.include <bsd.kmod.mk>
