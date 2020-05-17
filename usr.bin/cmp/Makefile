#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/cmp/Makefile 272780 2014-10-09 00:50:33Z ngie $

.include <src.opts.mk>

PROG=	cmp
SRCS=	cmp.c link.c misc.c regular.c special.c

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
