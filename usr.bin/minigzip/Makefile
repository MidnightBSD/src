# $FreeBSD: stable/9/usr.bin/minigzip/Makefile 237691 2012-06-28 07:08:48Z delphij $

SRCDIR=	${.CURDIR}/../../lib/libz/test
.PATH:	${SRCDIR}

PROG=	minigzip

WARNS?=	5
CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
