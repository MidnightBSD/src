# $FreeBSD: stable/11/lib/libcasper/services/cap_dns/Makefile 322715 2017-08-20 06:07:40Z ngie $

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper
LIB=	cap_dns

SHLIB_MAJOR=	0
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_dns.c

INCS=	cap_dns.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
