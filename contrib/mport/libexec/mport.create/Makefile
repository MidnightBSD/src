PROG= mport.create

CFLAGS+=	-fblocks -I${.CURDIR}/../../libmport/
WARNS?= 	6

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

LDFLAGS += -L../libmport -lmport -ldispatch -lBlocksRuntime -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
