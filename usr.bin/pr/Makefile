# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/pr/Makefile 319257 2017-05-30 22:02:55Z asomers $

.include <bsd.own.mk>

PROG=	pr
SRCS=	pr.c egetopt.c

.if ${MK_TESTS} != "no"
SUBDIR+=        tests
.endif

.include <bsd.prog.mk>
