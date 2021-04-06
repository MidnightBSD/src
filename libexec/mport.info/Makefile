PROG=	mport.info

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.info
 
CFLAGS+=       -I${MPORTDIR}/libmport 

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
