# $FreeBSD: release/7.0.0/tools/build/Makefile 146261 2005-05-16 09:32:41Z ru $

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRCS=
INCSGROUPS=	INCS
INCS=		

BOOTSTRAPPING?=	0

# usr.bin/gencat needs <nl_types.h>.
.if ${BOOTSTRAPPING} < 600017
INCS+=		nl_types.h
.endif

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

.include <bsd.lib.mk>
