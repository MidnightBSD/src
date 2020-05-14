#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/pr/Makefile 317439 2017-04-26 14:44:39Z asomers $

.include <src.opts.mk>

PROG=	pr
SRCS=	pr.c egetopt.c

.if ${MK_TESTS} != "no"
SUBDIR+=        tests
.endif

.include <bsd.prog.mk>
