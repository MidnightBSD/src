# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/linprocfs/Makefile 293527 2016-01-09 16:08:22Z dchagin $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h \
	linprocfs.c

.include <bsd.kmod.mk>
