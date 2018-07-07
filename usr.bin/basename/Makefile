# $MidnightBSD$
#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/basename/Makefile 276490 2015-01-01 02:04:44Z ngie $

.include <bsd.own.mk>

PROG=	basename
MLINKS=	basename.1 dirname.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
