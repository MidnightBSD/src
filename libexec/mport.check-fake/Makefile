# $MidnightBSD$

PROG= mport.check-fake

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -fblocks
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport -pthread -ldispatch -lBlocksRuntime
DPADD= 	${LIBMPORT} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME}

.include <bsd.prog.mk>
