# $MidnightBSD: src/usr.bin/msearch/Makefile,v 1.5 2011/08/07 02:58:18 laffer1 Exp $

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

WARNS?=	1

DPADD=  ${LIBMSEARCH} ${LIBZ} ${LIBSQLITE3}
LDADD=  -lmsearch -lz -lsqlite3

.include <bsd.prog.mk>
