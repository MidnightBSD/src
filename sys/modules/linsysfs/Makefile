# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/linsysfs/Makefile 293527 2016-01-09 16:08:22Z dchagin $

.PATH: ${.CURDIR}/../../compat/linsysfs

KMOD=	linsysfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	linsysfs.c

.include <bsd.kmod.mk>
