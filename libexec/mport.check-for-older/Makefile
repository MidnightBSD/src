PROG= mport.check-for-older

CFLAGS+=	-I ../../libmport/
WARNS?= 	4

MK_MAN= no

LIBADD= mport pthread

LDFLAGS += -L../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
