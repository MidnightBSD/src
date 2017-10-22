# $FreeBSD: release/10.0.0/sys/modules/nullfs/Makefile 89260 2002-01-11 15:49:02Z ru $

.PATH: ${.CURDIR}/../../fs/nullfs

KMOD=	nullfs
SRCS=	vnode_if.h \
	null_subr.c null_vfsops.c null_vnops.c

.include <bsd.kmod.mk>
