PROG= mport.create

CFLAGS+=	-fblocks -I${.CURDIR}/../../lib/libmport/
WARNS?= 	6

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
