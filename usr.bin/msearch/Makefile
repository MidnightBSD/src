# $MidnightBSD: src/usr.sbin/mport/Makefile,v 1.5 2011/05/21 19:26:24 laffer1 Exp $

PROG=	msearch

CFLAGS= -I${.CURDIR}/../../lib/libsearch -g

NO_MAN=	

DPADD=  ${LIBSEARCH}
LDADD=  -lmsearch

.include <bsd.prog.mk>
