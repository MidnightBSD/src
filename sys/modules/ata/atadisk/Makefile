# $FreeBSD: stable/9/sys/modules/ata/atadisk/Makefile 208468 2010-05-23 20:24:03Z marius $

.PATH: ${.CURDIR}/../../../dev/ata ${.CURDIR}/../../../${MACHINE}/${MACHINE}

KMOD=	atadisk
SRCS=	ata-disk.c ${ata_machdep}
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.if exists(${.CURDIR}/../../../${MACHINE}/${MACHINE}/ata_machdep.c)
ata_machdep=	ata_machdep.c
.endif

.include <bsd.kmod.mk>
