# $MidnightBSD$

PROG= mport.query

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -fblocks
WARNS?= 	4

NO_MAN=		yes

LDADD= 	-lmport -ldispatch -lBlocksRuntime
DPADD= 	${LIBMPORT} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME}

.include <bsd.prog.mk>
