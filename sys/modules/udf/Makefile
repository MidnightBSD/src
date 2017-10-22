# $FreeBSD: release/10.0.0/sys/modules/udf/Makefile 124472 2004-01-13 11:28:51Z ru $

.PATH: ${.CURDIR}/../../fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv

.include <bsd.kmod.mk>
