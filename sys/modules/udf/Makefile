# $FreeBSD: stable/11/sys/modules/udf/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/fs/udf

KMOD=	udf

SRCS=	udf_vfsops.c udf_vnops.c osta.c
SRCS+=	vnode_if.h
EXPORT_SYMS=	udf_iconv

.include <bsd.kmod.mk>
