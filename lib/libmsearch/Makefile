# $MidnightBSD: src/lib/libmsearch/Makefile,v 1.2 2011/08/04 01:11:39 laffer1 Exp $

LIB=	msearch
SRCS=	db.c msearch_fulltext.c msearch_index.c msearch_search.c
INCS=	msearch.h

CFLAGS+=	-I${.CURDIR} -g 
WARNS?=		6
WFORMAT?=       1
SHLIB_MAJOR=	1

DPADD=  ${LIBSQLITE3} ${LIBMAGIC}
LDADD=  -lsqlite3 -lmagic

.include <bsd.lib.mk>
