# $FreeBSD: stable/11/sys/modules/msdosfs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/fs/msdosfs

KMOD=	msdosfs
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_fileno.c \
	msdosfs_lookup.c msdosfs_vfsops.c msdosfs_vnops.c
EXPORT_SYMS=	msdosfs_iconv

.include <bsd.kmod.mk>
