# $MidnightBSD$
# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapicd
SRCS=	atapi-cd.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
