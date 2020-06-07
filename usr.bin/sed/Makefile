#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/sed/Makefile 276837 2015-01-08 17:23:59Z pfg $

.include <src.opts.mk>

PROG=	sed
SRCS=	compile.c main.c misc.c process.c

WARNS?=	2

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
