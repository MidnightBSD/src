# $FreeBSD: release/10.0.0/sys/modules/dpms/Makefile 198251 2009-10-19 20:58:10Z jkim $

KMOD=	dpms
SRCS=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/dpms
SRCS+=	dpms.c

.include <bsd.kmod.mk>
