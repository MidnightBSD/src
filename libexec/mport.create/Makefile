PROG= mport.create

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/
WARNS?= 	4

NO_MAN=		yes

LDADD= 	${LIBMPORT} ${LIBSQLITE3} ${LIBPTHREAD} ${LIBMD}
DPADD= 	${LIBMPORT} ${LIBSQLITE3} ${LIBPTHREAD} ${LIBMD}

.include <bsd.prog.mk>
