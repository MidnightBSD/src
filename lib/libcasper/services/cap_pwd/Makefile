# $FreeBSD: stable/11/lib/libcasper/services/cap_pwd/Makefile 322715 2017-08-20 06:07:40Z ngie $

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper
LIB=	cap_pwd

SHLIB_MAJOR=	0
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_pwd.c

INCS=	cap_pwd.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
