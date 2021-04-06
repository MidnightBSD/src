PROG= mport.install

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.install
 
CFLAGS+=       -I${MPORTDIR}/libmport 

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
