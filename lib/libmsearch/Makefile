# $MidnightBSD: src/lib/libmsearch/Makefile,v 1.4 2011/08/06 23:02:51 laffer1 Exp $

LIB=	msearch
SRCS=	db.c msearch_fulltext.c msearch_index.c msearch_search.c sqlite_functions.c
INCS=	msearch.h

CFLAGS+=	-I${.CURDIR} -g 
WARNS?=		3
WFORMAT?=       1
SHLIB_MAJOR=	1

DPADD=  ${LIBSQLITE3} ${LIBMAGIC}
LDADD=  -lsqlite3 -lmagic

.include <bsd.lib.mk>
