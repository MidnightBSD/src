PROG= mport.query

CFLAGS+=	-I${.CURDIR}/../../libmport/ -I/usr/include/private/ucl -I ../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	4

MK_MAN= no

LIBADD= mport pthread sqlite3

LDFLAGS += -L../libmport -lmport -lpthread -lsqlite3

BINDIR=/usr/libexec

.include <bsd.prog.mk>
