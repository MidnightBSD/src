# $FreeBSD: release/7.0.0/sys/modules/aio/Makefile 163379 2006-10-15 14:22:14Z netchild $

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue

.include <bsd.kmod.mk>
