PROG= mport.query
MAN= mport.query.1

CFLAGS+=	-I${.CURDIR}/../../libmport/ -I/usr/include/private/ucl -I ${.CURDIR}/../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	4

LIBADD= mport pthread sqlite3

LDFLAGS += -L ${.CURDIR}/../../libmport -lmport -lpthread -lsqlite3

BINDIR=/usr/libexec

.include <bsd.prog.mk>
