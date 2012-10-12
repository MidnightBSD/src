# $DragonFly: src/sys/dev/netif/et/Makefile,v 1.1 2007/10/12 14:12:42 sephe Exp $
# $MidnightBSD: src/sys/modules/et/Makefile,v 1.1 2011/10/19 15:17:28 laffer1 Exp $

.PATH: ${.CURDIR}/../../dev/et

KMOD=	if_et
SRCS=	if_et.c
SRCS+=	bus_if.h pci_if.h device_if.h miibus_if.h

.include <bsd.kmod.mk>
