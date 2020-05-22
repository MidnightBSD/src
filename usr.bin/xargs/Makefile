#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/xargs/Makefile 265420 2014-05-06 04:22:01Z imp $

.include <src.opts.mk>

PROG=	xargs
SRCS=	xargs.c strnsubst.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
