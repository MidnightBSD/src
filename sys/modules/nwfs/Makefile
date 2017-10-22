# $FreeBSD: stable/9/sys/modules/nwfs/Makefile 154865 2006-01-26 19:21:27Z alc $

.PATH: ${.CURDIR}/../../fs/nwfs

KMOD=	nwfs

SRCS=	vnode_if.h \
	nwfs_node.c nwfs_ioctl.c nwfs_io.c nwfs_vfsops.c nwfs_vnops.c \
	nwfs_subr.c

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

.include <bsd.kmod.mk>
