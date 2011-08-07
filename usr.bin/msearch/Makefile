# $MidnightBSD: src/usr.bin/msearch/Makefile,v 1.3 2011/08/07 01:43:52 laffer1 Exp $

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

WARNS?=	1
NO_MAN=	

DPADD=  ${LIBSEARCH}
LDADD=  -lmsearch

.include <bsd.prog.mk>
