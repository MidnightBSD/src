# $FreeBSD: stable/11/lib/libnv/Makefile 337561 2018-08-10 01:43:05Z kevans $

PACKAGE=lib${LIB}
SHLIBDIR?= /lib

.include <src.opts.mk>

LIB=	nv
SHLIB_MAJOR= 0

.PATH: ${SRCTOP}/sys/contrib/libnv ${SRCTOP}/sys/sys
CFLAGS+=-I${.CURDIR}

SRCS=	cnvlist.c
SRCS+=	dnvlist.c
SRCS+=	msgio.c
SRCS+=	nvlist.c
SRCS+=	nvpair.c

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
