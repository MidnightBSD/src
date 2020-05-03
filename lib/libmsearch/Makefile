# $MidnightBSD: src/lib/libmsearch/Makefile,v 1.7 2011/08/07 06:17:57 laffer1 Exp $

LIB=	msearch
SRCS=	db.c msearch_fulltext.c msearch_index.c msearch_search.c sqlite_functions.c
INCS=	msearch.h

CFLAGS+=	-I${.CURDIR}
WARNS?=		3
WFORMAT?=       1
SHLIB_MAJOR=	1
CSTD=	c99

LIBADD+=	sqlite3 magic z

.include <bsd.lib.mk>
