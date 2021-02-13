PROG= mport.delete

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.delete
 
CFLAGS+=       -I${MPORTDIR}/libmport -fblocks

WARNS?= 	4

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
