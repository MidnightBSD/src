# $FreeBSD: stable/11/usr.bin/tail/Makefile 314427 2017-02-28 22:49:41Z asomers $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <src.opts.mk>

PROG=	tail
SRCS=	forward.c misc.c read.c reverse.c tail.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
