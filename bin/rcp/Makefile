#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: stable/11/bin/rcp/Makefile 326299 2017-11-28 00:39:58Z asomers $

.include <src.opts.mk>

PACKAGE=rcmds
PROG=	rcp
SRCS=	rcp.c util.c
CFLAGS+=-DBINDIR=${BINDIR}

PACKAGE=rcmds

BINOWN=	root
BINMODE=4555

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
