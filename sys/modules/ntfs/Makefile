# $FreeBSD: stable/9/sys/modules/ntfs/Makefile 120492 2003-09-26 20:26:25Z fjoe $

.PATH: ${.CURDIR}/../../fs/ntfs

KMOD=	ntfs
SRCS=	vnode_if.h \
	ntfs_vfsops.c ntfs_vnops.c ntfs_subr.c ntfs_ihash.c \
	ntfs_compr.c
EXPORT_SYMS=	ntfs_iconv

.include <bsd.kmod.mk>
