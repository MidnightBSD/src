# $MidnightBSD: src/usr.bin/msearch/Makefile,v 1.6 2011/08/07 06:25:00 laffer1 Exp $

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

WARNS?=	1
CSTD=	c99

DPADD=  ${LIBMSEARCH} ${LIBZ} ${LIBSQLITE3}
LDADD=  -lmsearch -lz -lsqlite3

.include <bsd.prog.mk>
