#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: stable/11/usr.bin/compress/Makefile 319163 2017-05-29 21:48:02Z jilles $

.include <src.opts.mk>

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
