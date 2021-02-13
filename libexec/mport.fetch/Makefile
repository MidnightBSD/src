PROG= mport.fetch

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.fetch
 
CFLAGS+=       -I${MPORTDIR}/libmport -fblocks

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
