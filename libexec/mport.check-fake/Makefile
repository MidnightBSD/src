PROG= mport.check-fake

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ 
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport -pthread
DPADD= 	${LIBMPORT}

.include <bsd.prog.mk>
