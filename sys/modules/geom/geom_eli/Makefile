# $FreeBSD: release/7.0.0/sys/modules/geom/geom_eli/Makefile 159309 2006-06-05 21:42:26Z pjd $

.PATH: ${.CURDIR}/../../../geom/eli

KMOD=	geom_eli
SRCS=	g_eli.c
SRCS+=	g_eli_crypto.c
SRCS+=	g_eli_ctl.c
SRCS+=	g_eli_integrity.c
SRCS+=	g_eli_key.c
SRCS+=	g_eli_privacy.c
SRCS+=	pkcs5v2.c
SRCS+=	vnode_if.h
WARNS?=	2

.include <bsd.kmod.mk>
