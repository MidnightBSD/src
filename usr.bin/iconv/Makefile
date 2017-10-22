# $FreeBSD: release/10.0.0/usr.bin/iconv/Makefile 222772 2011-06-06 20:24:17Z ed $
# $NetBSD: Makefile,v 1.6 2009/04/14 22:15:21 lukem Exp $

.include <bsd.own.mk>

PROG=	iconv
#SRCS=	iconv.c
MAN=	iconv.1

LDADD+=	-lcrypt
DPADD+=	${LIBCRYPT}

.include <bsd.prog.mk>
