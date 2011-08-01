# $MidnightBSD: src/usr.bin/msearch/Makefile,v 1.1 2011/07/24 15:11:27 laffer1 Exp $

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch -g

NO_MAN=	

DPADD=  ${LIBSEARCH}
LDADD=  -lmsearch

.include <bsd.prog.mk>
