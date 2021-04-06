PROG= mport.delete

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.delete
 
CFLAGS+=       -I${MPORTDIR}/libmport 

WARNS?= 	4

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
