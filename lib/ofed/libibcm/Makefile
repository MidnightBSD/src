# $FreeBSD: stable/11/lib/ofed/libibcm/Makefile 335253 2018-06-16 15:05:05Z kib $

_spath=	${SRCTOP}/contrib/ofed/libibcm
.PATH: ${_spath}

SHLIBDIR?=	/usr/lib
LIB=		ibcm
SHLIB_MAJOR=	1
MK_PROFILE=	no
SRCS=		cm.c
LIBADD=		ibverbs
CFLAGS+=	-I${_spath}
MAN=
VERSION_MAP=	${_spath}/libibcm.map

.include <bsd.lib.mk>
