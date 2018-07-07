# $MidnightBSD$
#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: stable/10/usr.bin/compress/Makefile 320518 2017-06-30 22:16:28Z jilles $

.include <bsd.own.mk>

PROG=	compress
SRCS=	compress.c zopen.c
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS=	compress.1 uncompress.1

# XXX zopen is not part of libc
# MAN=zopen.3

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
