# $MidnightBSD: src/gnu/lib/libssp/Makefile,v 1.1 2009/05/22 00:27:23 laffer1 Exp $

SHLIBDIR?=	/lib

.include <bsd.own.mk>
MK_SSP=		no

GCCDIR=	${.CURDIR}/../../../contrib/gcc4
GCCLIB=	${.CURDIR}/../../../contrib/gcclibs
SRCDIR=	${GCCLIB}/libssp

.PATH: ${SRCDIR} ${SRCDIR}/ssp

LIB=		ssp
SHLIB_MAJOR=	0
NO_PROFILE=

SRCS=	ssp.c gets-chk.c memcpy-chk.c memmove-chk.c mempcpy-chk.c \
	memset-chk.c snprintf-chk.c sprintf-chk.c stpcpy-chk.c \
	strcat-chk.c strcpy-chk.c strncat-chk.c strncpy-chk.c \
	vsnprintf-chk.c vsprintf-chk.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR}  -I${SRCDIR}  -I${GCCLIB}/include

VERSION_MAP= ${SRCDIR}/ssp.map


INCS=	ssp.h string.h stdio.h unistd.h
INCSDIR=${INCLUDEDIR}/ssp

CLEANFILES= ssp.h
ssp.h: ssp.h.in
	sed -e 's/@ssp_have_usable_vsnprintf@/define/' ${.ALLSRC} > ${.TARGET}

SUBDIR+= libssp_nonshared

.include <bsd.lib.mk>
