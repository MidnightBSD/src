PROG= mport.create

CFLAGS+=	-I${.CURDIR}/../../libmport/
WARNS?= 	6

MK_MAN= no

LIBADD= mport pthread

LDFLAGS += -L../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
