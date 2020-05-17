# $FreeBSD: stable/11/usr.bin/cxxfilt/Makefile 298401 2016-04-21 12:58:29Z emaste $

.include <src.opts.mk>

ELFTCDIR=	${SRCTOP}/contrib/elftoolchain
SRCDIR=		${ELFTCDIR}/cxxfilt

.PATH: ${SRCDIR}

PROG=	c++filt
SRCS=	cxxfilt.c

LIBADD=	elftc

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
