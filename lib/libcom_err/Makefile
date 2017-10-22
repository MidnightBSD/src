# $FreeBSD: release/10.0.0/lib/libcom_err/Makefile 233294 2012-03-22 08:48:42Z stas $

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	${COM_ERRDIR}/com_err.h ${COM_ERRDIR}/com_right.h
MAN=	com_err.3
COM_ERRDIR=	${.CURDIR}/../../contrib/com_err
CFLAGS+=	-I${COM_ERRDIR}

LDFLAGS=	-Wl,--no-undefined
VERSION_MAP=	${COM_ERRDIR}/version-script.map

SUBDIR=	doc

.include <bsd.lib.mk>

.PATH:	${COM_ERRDIR}
