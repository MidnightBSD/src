PROG= mport.update

CFLAGS+=	-I${.CURDIR}/../../libmport/ -fblocks
WARNS?= 	4

MK_MAN= no

LIBADD= mport pthread

LDFLAGS += -L../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
