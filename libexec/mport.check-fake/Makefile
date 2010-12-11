PROG= mport.check-fake

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -g -DPRINT_DIAG
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport
DPADD= 	${LIBMPORT}

.include <bsd.prog.mk>
