PROG= mport.install

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/
WARNS?= 	4

NO_MAN=		yes

LDADD= 	${LIBMPORT} ${LIBSQLITE3} ${LIBMD} ${LIBARCHIVE} ${LIBBZ2}
DPADD= 	${LIBMPORT} ${LIBSQLITE3} ${LIBMD} ${LIBARCHIVE} ${LIBBZ2}

.include <bsd.prog.mk>
