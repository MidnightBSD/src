PROG= mport.install

CFLAGS+=	-I${.CURDIR}/../../lib/libmport/ -fblocks
WARNS?= 	6

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
