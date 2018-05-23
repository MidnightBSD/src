# $MidnightBSD$

PROG= mport

CFLAGS= -I${.CURDIR}/../../lib/libmport/ -fblocks -g

DPADD=  ${LIBMPORT} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME}
LDADD=  -lmport -ldispatch -lBlocksRuntime

.include <bsd.prog.mk>
