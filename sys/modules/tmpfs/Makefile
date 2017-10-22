# $FreeBSD: release/10.0.0/sys/modules/tmpfs/Makefile 171087 2007-06-29 05:23:15Z delphij $

.PATH: ${.CURDIR}/../../fs/tmpfs

KMOD=	tmpfs
SRCS=	vnode_if.h \
	tmpfs_vnops.c tmpfs_fifoops.c tmpfs_vfsops.c tmpfs_subr.c

.include <bsd.kmod.mk>
