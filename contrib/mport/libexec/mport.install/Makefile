PROG= mport.install

CFLAGS+=	-I${.CURDIR}/../../libmport/ -fblocks
WARNS?= 	6

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

LDFLAGS += -L../libmport -lmport -ldispatch -lBlocksRuntime -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
