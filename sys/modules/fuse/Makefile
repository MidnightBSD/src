# $FreeBSD: stable/11/sys/modules/fuse/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/fs/fuse

KMOD=	fuse
SRCS=	vnode_if.h \
	fuse_node.c fuse_io.c fuse_device.c fuse_ipc.c fuse_file.c \
	fuse_vfsops.c fuse_vnops.c fuse_internal.c fuse_main.c

.include <bsd.kmod.mk>
