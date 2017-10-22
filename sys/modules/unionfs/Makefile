# $FreeBSD: release/10.0.0/sys/modules/unionfs/Makefile 89260 2002-01-11 15:49:02Z ru $

.PATH: ${.CURDIR}/../../fs/unionfs

KMOD=	unionfs
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c

.include <bsd.kmod.mk>
