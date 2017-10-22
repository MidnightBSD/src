# $FreeBSD: release/10.0.0/sys/modules/cd9660/Makefile 166640 2007-02-11 14:01:32Z rodrigc $

.PATH: ${.CURDIR}/../../fs/cd9660

KMOD=	cd9660
SRCS=	vnode_if.h \
	cd9660_bmap.c cd9660_lookup.c cd9660_node.c cd9660_rrip.c \
	cd9660_util.c cd9660_vfsops.c cd9660_vnops.c
EXPORT_SYMS=	cd9660_iconv

.include <bsd.kmod.mk>
