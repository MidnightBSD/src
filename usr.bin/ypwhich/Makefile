#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: stable/11/usr.bin/ypwhich/Makefile 319186 2017-05-30 04:27:05Z ngie $

YPSERV=${SRCTOP}/usr.sbin/ypserv/common
.PATH: ${YPSERV}

PROG=	ypwhich
SRCS=	yplib_host.c ypwhich.c

CFLAGS+=	-I${YPSERV} -I.

WARNS?=	2

.include <bsd.prog.mk>
