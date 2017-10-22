# $FreeBSD: release/10.0.0/sys/modules/fuse/Makefile 241519 2012-10-13 23:54:26Z attilio $

.PATH: ${.CURDIR}/../../fs/fuse

KMOD=	fuse
SRCS=	vnode_if.h \
	fuse_node.c fuse_io.c fuse_device.c fuse_ipc.c fuse_file.c \
	fuse_vfsops.c fuse_vnops.c fuse_internal.c fuse_main.c

.include <bsd.kmod.mk>
