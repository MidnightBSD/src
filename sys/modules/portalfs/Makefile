# $FreeBSD: stable/9/sys/modules/portalfs/Makefile 224876 2011-08-15 07:32:44Z rwatson $

.PATH: ${.CURDIR}/../../fs/portalfs

KMOD=	portalfs
SRCS=	vnode_if.h \
	portal_vfsops.c portal_vnops.c \
	opt_capsicum.h

.include <bsd.kmod.mk>
