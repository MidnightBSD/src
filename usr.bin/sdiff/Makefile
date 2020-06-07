# $FreeBSD: stable/11/usr.bin/sdiff/Makefile 307772 2016-10-22 13:15:19Z bapt $

.include <src.opts.mk>

PROG=	sdiff
SRCS=	edit.c sdiff.c
WARNS=	3

MAN1=	sdiff.1

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
