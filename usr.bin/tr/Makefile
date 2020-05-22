#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/tr/Makefile 265420 2014-05-06 04:22:01Z imp $

.include <src.opts.mk>

PROG=	tr
SRCS=	cmap.c cset.c str.c tr.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
