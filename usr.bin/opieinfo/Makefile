# $FreeBSD: stable/11/usr.bin/opieinfo/Makefile 319186 2017-05-30 04:27:05Z ngie $
#
OPIE_DIST?=	${SRCTOP}/contrib/opie

PROG=	opieinfo

CFLAGS+=-I${SRCTOP}/lib/libopie
CFLAGS+=-I${OPIE_DIST}
CFLAGS+=-DINSECURE_OVERRIDE

WARNS?=	0

LIBADD=	opie

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
