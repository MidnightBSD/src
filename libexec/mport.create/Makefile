PROG= mport.create

CFLAGS+=	-fblocks -I${.CURDIR}/../../lib/libmport/
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport -ldispatch -lBlocksRuntime
DPADD= 	${LIBMPORT} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME}

.include <bsd.prog.mk>
