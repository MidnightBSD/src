PROG=	mport.info

CFLAGS+=-I${.CURDIR}/../../libmport/ -I/usr/include/private/ucl
WARNS?= 4

MK_MAN= no

LIBADD= mport pthread

LDFLAGS += -L../libmport -lmport -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
