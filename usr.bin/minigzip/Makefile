# $FreeBSD: release/10.0.0/usr.bin/minigzip/Makefile 237410 2012-06-21 21:47:08Z delphij $

SRCDIR=	${.CURDIR}/../../lib/libz/test
.PATH:	${SRCDIR}

PROG=	minigzip

WARNS?=	5
CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
