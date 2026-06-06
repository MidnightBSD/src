PROG= mport.install
MAN= mport.install.1

CFLAGS+=	-I${.CURDIR}/../../libmport/ -I/usr/include/private/ucl -I ${.CURDIR}/../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	6

LIBADD= mport pthread

LDFLAGS += -L ${.CURDIR}/../../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
