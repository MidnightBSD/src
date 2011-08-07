# $MidnightBSD: src/usr.bin/msearch/Makefile,v 1.2 2011/08/01 22:51:15 laffer1 Exp $

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

NO_MAN=	

DPADD=  ${LIBSEARCH}
LDADD=  -lmsearch

.include <bsd.prog.mk>
