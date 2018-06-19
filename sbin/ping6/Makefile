# $MidnightBSD$
# $FreeBSD: stable/10/sbin/ping6/Makefile 287328 2015-08-31 18:53:41Z delphij $

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DIPSEC -DKAME_SCOPEID -DUSE_RFC2292BIS
WARNS?=	2

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lm -lmd
DPADD= ${LIBIPSEC} ${LIBM} ${LIBMD}

.include <bsd.prog.mk>
