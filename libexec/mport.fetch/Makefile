PROG= mport.fetch

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.fetch
 
CFLAGS+=       -I${MPORTDIR}/libmport 

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
