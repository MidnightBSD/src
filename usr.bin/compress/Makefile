#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: release/10.0.0/usr.bin/compress/Makefile 201386 2010-01-02 10:27:05Z ed $

PROG=	compress
SRCS=	compress.c zopen.c
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS=	compress.1 uncompress.1

# XXX zopen is not part of libc
# MAN=zopen.3

.include <bsd.prog.mk>
