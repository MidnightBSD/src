# $MidnightBSD: src/lib/libmsearch/Makefile,v 1.1 2011/07/24 15:07:37 laffer1 Exp $

LIB=	msearch
SRCS=	db.c msearch_fulltext.c msearch_index.c msearch_search.c
INCS=	msearch.h

CFLAGS+=	-I${.CURDIR} -g
WARNS?=		6
WFORMAT?=       1
SHLIB_MAJOR=	1

DPADD=  ${LIBSQLITE3}
LDADD=  -lsqlite3

.include <bsd.lib.mk>
