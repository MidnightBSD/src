PROG= mport.query

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -fblocks
WARNS?= 	4

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread sqlite3

.include <bsd.prog.mk>
