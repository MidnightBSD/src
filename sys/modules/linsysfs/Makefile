# $FreeBSD: stable/11/sys/modules/linsysfs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/compat/linsysfs

KMOD=	linsysfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	linsysfs.c

.include <bsd.kmod.mk>
