#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: stable/10/bin/rcp/Makefile 326332 2017-11-28 18:18:39Z asomers $
# $MidnightBSD$

.include <bsd.own.mk>

PACKAGE=rcmds
PROG=	rcp
SRCS=	rcp.c util.c
CFLAGS+=-DBINDIR=${BINDIR}

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
