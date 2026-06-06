PROG= mport.list
MAN= mport.list.1

CFLAGS+=	-I${.CURDIR}/../../libmport/ -I ${.CURDIR}/../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	4

LIBADD= mport pthread

LDFLAGS += -L${.CURDIR}/../../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
