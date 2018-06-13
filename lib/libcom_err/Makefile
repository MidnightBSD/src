# $MidnightBSD$
# $FreeBSD: stable/10/lib/libcom_err/Makefile 313622 2017-02-11 06:58:24Z ngie $

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	${COM_ERRDIR}/com_err.h ${COM_ERRDIR}/com_right.h
MAN=	com_err.3
COM_ERRDIR=	${SRCTOP}/contrib/com_err
CFLAGS+=	-I${COM_ERRDIR}

LDFLAGS=	-Wl,--no-undefined
VERSION_MAP=	${COM_ERRDIR}/version-script.map

SUBDIR=	doc

.include <bsd.lib.mk>

.PATH:	${COM_ERRDIR}
