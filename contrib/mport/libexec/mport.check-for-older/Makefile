PROG= mport.check-for-older

CFLAGS+=	-I ${.CURDIR}/../../libmport/ -I/usr/include/private/ucl -I ${.CURDIR}/../../external/tllist/ -I/usr/include/private/zstd 
WARNS?= 	4

MK_MAN= no

LIBADD= mport pthread

LDFLAGS += -L ${.CURDIR}/../../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
