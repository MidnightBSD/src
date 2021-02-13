PROG= mport.create

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.create
 
CFLAGS+=       -I${MPORTDIR}/libmport -fblocks

WARNS?= 	6

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
