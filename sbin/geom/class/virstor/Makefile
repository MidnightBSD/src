# $FreeBSD: release/7.0.0/sbin/geom/class/virstor/Makefile 172302 2007-09-23 07:34:23Z pjd $

.PATH:	${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/virstor

CLASS=	virstor

SRCS+=	binstream.c
SRCS+=	g_virstor_md.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.lib.mk>
