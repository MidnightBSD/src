# $MidnightBSD: src/usr.bin/msearch/Makefile,v 1.4 2011/08/07 02:37:02 laffer1 Exp $

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

WARNS?=	1

DPADD=  ${LIBSEARCH}
LDADD=  -lmsearch

.include <bsd.prog.mk>
