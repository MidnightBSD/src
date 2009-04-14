PROG= mport.check-for-older

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/
WARNS?= 	4

NO_MAN=		yes

LDADD= 	-lmport
DPADD= 	${LIBMPORT} 

.include <bsd.prog.mk>
