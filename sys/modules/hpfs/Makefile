# $FreeBSD: stable/9/sys/modules/hpfs/Makefile 144214 2005-03-28 09:41:25Z jeff $

.PATH: ${.CURDIR}/../../fs/hpfs

KMOD=	hpfs
SRCS=	vnode_if.h \
	hpfs_vfsops.c hpfs_vnops.c hpfs_subr.c hpfs_lookup.c \
	hpfs_alsubr.c

.include <bsd.kmod.mk>
