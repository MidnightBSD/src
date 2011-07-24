# $MidnightBSD$

LIB=	msearch
SRCS=	msearch_index.c msearch_search.c db.c
INCS=	msearch.h

CFLAGS+=	-I${.CURDIR} -g
WARNS?=		6
WFORMAT?=       1
SHLIB_MAJOR=	1

DPADD=  ${LIBSQLITE3}
LDADD=  -lsqlite3

.include <bsd.lib.mk>
