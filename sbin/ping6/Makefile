# $FreeBSD: stable/11/sbin/ping6/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DIPSEC -DKAME_SCOPEID -DUSE_RFC2292BIS
WARNS?=	3

BINOWN=	root
BINMODE=4555

LIBADD=	ipsec m md

.include <bsd.prog.mk>
