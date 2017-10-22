# $FreeBSD: release/10.0.0/sys/modules/ida/Makefile 124367 2004-01-11 07:01:30Z mdodd $

.PATH:	${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c \
	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
