# $FreeBSD: release/10.0.0/tools/test/iconv/gnu/Makefile 219019 2011-02-25 00:04:39Z gabor $

PROG=	gnu
NO_MAN=	yes

WARNS?=	3
CFLAGS+= -L/usr/local/lib -I/usr/local/include
LDADD=	-liconv
DPADD=	/usr/local/lib/libiconv.a

.include <bsd.prog.mk>
