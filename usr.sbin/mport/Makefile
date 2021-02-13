PROG= mport

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/mport

CFLAGS= -I${MPORTDIR}/libmport/ -fblocks -g

LIBADD+=	mport dispatch BlocksRuntime

.include <bsd.prog.mk>
