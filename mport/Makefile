PROG= mport

CFLAGS= -I ${.CURDIR}/../libmport/ -I ${.CURDIR}/../external/tllist/ -I/usr/include/private/zstd  -I/usr/include/private/ucl -g 
CFLAGS+= -Werror -Wunused-variable -Wshadow -Wincompatible-pointer-types-discards-qualifiers

LDFLAGS+= -L ${.CURDIR}/../libmport -lmport -lutil
LIBADD+=	mport util

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
