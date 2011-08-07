# $MidnightBSD: src/libexec/msearch.index/Makefile,v 1.2 2011/08/07 01:43:09 laffer1 Exp $

PROG=	msearch.index

CFLAGS+=-I${.CURDIR}/../../lib/libmsearch
WARNS?=	6

NO_MAN=	yes

LDADD=	-lmsearch -lz -lsqlite3
DPADD=	${LIBMPORT} ${LIBZ} ${LIBSQLITE3}

.include <bsd.prog.mk>
