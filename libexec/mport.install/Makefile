PROG= mport.install

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -O0 -g
WARNS?= 	6

NO_MAN=		yes

LDADD= 	-lmport
DPADD= 	${LIBMPORT} 

.include <bsd.prog.mk>
