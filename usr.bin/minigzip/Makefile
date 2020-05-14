# $FreeBSD: stable/11/usr.bin/minigzip/Makefile 348261 2019-05-25 01:58:00Z delphij $

SRCDIR=	${SRCTOP}/sys/contrib/zlib/test
.PATH:	${SRCDIR}

PROG=	minigzip

WARNS?=	5
CFLAGS+=-DUSE_MMAP
LIBADD=	z

.include <bsd.prog.mk>
