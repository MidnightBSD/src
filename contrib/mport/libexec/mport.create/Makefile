PROG= mport.create
MAN= mport.create.1

CFLAGS+=	-I${.CURDIR}/../../libmport/ -I/usr/include/private/ucl -I ${.CURDIR}/../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	6

LIBADD= mport pthread privateucl

LDFLAGS += -L ${.CURDIR}/../../libmport -lmport -lpthread -lprivateucl

BINDIR=/usr/libexec

.include <bsd.prog.mk>
