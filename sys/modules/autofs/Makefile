# $FreeBSD: stable/11/sys/modules/autofs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/fs/autofs

KMOD=	autofs
SRCS=	vnode_if.h \
	autofs.c \
	autofs_vnops.c \
	autofs_vfsops.c

.include <bsd.kmod.mk>
