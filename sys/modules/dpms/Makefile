# $MidnightBSD: src/sys/modules/dpms/Makefile,v 1.1 2012/03/31 16:59:45 laffer1 Exp $
# $FreeBSD: src/sys/modules/dpms/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../i386/isa

KMOD=	dpms
SRCS=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/dpms
SRCS+=	dpms.c

.include <bsd.kmod.mk>
