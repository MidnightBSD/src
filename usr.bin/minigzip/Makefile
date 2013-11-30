# $MidnightBSD$
# $FreeBSD: src/usr.bin/minigzip/Makefile,v 1.4 2004/02/05 22:44:25 ru Exp $

SRCDIR=	${.CURDIR}/../../lib/libz
.PATH:	${SRCDIR}

PROG=	minigzip

WARNS?=	5
CFLAGS+=-DUSE_MMAP
LDADD=	-lz
DPADD=	${LIBZ}

.include <bsd.prog.mk>
