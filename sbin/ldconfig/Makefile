# $FreeBSD: stable/11/sbin/ldconfig/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${SRCTOP}/libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
