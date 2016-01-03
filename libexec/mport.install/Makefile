# $MidnightBSD$

PROG= mport.install

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -fblocks
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport -ldispatch -lBlocksRuntime
DPADD= 	${LIBMPORT}  ${LIBBLOCKSRUNTIME} ${LIBDISPATCH}

.include <bsd.prog.mk>
