#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/basename/Makefile 272779 2014-10-09 00:41:42Z ngie $

.include <src.opts.mk>

PROG=	basename
MLINKS=	basename.1 dirname.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
