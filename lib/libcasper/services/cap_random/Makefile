# $FreeBSD: stable/11/lib/libcasper/services/cap_random/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=libcasper
LIB=	cap_random

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_random.c

INCS=	cap_random.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.include <bsd.lib.mk>
