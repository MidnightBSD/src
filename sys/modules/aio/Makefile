# $FreeBSD: release/10.0.0/sys/modules/aio/Makefile 185878 2008-12-10 20:56:19Z jhb $

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h opt_compat.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue

.include <bsd.kmod.mk>
