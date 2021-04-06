PROG= mport.check-for-older

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.check-for-older

CFLAGS+=       -I${MPORTDIR}/libmport 

WARNS?= 	4

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
