PROG= mport.list

CFLAGS+=	-I${.CURDIR}/../../libmport/ -I ../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	4

MK_MAN= no

LIBADD= mport pthread

LDFLAGS += -L${.CURDIR}/../../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
