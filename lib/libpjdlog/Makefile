#
# $FreeBSD: stable/11/lib/libpjdlog/Makefile 298107 2016-04-16 07:45:30Z gjb $
#

PACKAGE=lib${LIB}
SHLIBDIR?=	/lib

.include <bsd.own.mk>

LIB=		pjdlog
SRCS=		pjdlog.c

SHLIB_MAJOR=	0

CFLAGS+=-I${.CURDIR}

LIBADD=	util

.include <bsd.lib.mk>
