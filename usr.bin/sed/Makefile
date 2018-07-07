# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/sed/Makefile 264996 2014-04-27 01:15:10Z jmmv $

.include <bsd.own.mk>

PROG=	sed
SRCS=	compile.c main.c misc.c process.c

WARNS?=	2

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
