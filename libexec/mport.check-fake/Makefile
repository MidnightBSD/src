PROG= mport.check-fake

CFLAGS+=	-g -I${.CURDIR}/../../lib/libmport/ -DPRINT_DIAG
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport
DPADD= 	${LIBMPORT}

.include <bsd.prog.mk>
