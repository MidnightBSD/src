# $FreeBSD: stable/11/gnu/lib/libssp/libssp_nonshared/Makefile 264928 2014-04-25 19:25:26Z imp $

GCCDIR=	${.CURDIR}/../../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../../contrib/gcclibs

.PATH: ${GCCLIB}/libssp ${GCCLIB}/libssp/ssp

LIB=	ssp_nonshared
NO_PIC=
MK_PROFILE=no

SRCS=	ssp-local.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR}/..  -I${GCCLIB}/libssp  -I${GCCLIB}/include
CFLAGS+= -fPIC -DPIC -fvisibility=hidden

.include <bsd.lib.mk>
