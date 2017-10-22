# $FreeBSD: release/10.0.0/sys/modules/ep/Makefile 147959 2005-07-13 14:53:59Z imp $

.PATH: ${.CURDIR}/../../dev/ep

KMOD=	if_ep
SRCS=	if_ep.c
SRCS+=	if_ep_pccard.c pccarddevs.h card_if.h
SRCS+=	if_ep_isa.c isa_if.h
SRCS+=	if_ep_eisa.c eisa_if.h
#SRCS+=	if_ep_mca.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>
