# $MidnightBSD$

PROG= mport.query

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -fblocks
WARNS?= 	4

NO_MAN=		yes

LDADD= 	-lmport -ldispatch -lBlocksRuntime -lsqlite3
DPADD= 	${LIBMPORT} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME} ${LIBSQLITE3}

.include <bsd.prog.mk>
