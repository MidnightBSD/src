# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/tail/Makefile 314425 2017-02-28 22:18:05Z asomers $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

PROG=	tail
SRCS=	forward.c misc.c read.c reverse.c tail.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
