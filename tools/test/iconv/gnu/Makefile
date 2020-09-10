# $FreeBSD: stable/11/tools/test/iconv/gnu/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=	gnu
MAN=

WARNS?=	3
CFLAGS+= -L/usr/local/lib -I/usr/local/include
LDADD=	-liconv
DPADD=	/usr/local/lib/libiconv.a

.include <bsd.prog.mk>
