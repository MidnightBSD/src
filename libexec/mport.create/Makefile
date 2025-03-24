PROG= mport.create

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.create
 
CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/

WARNS?= 	6

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
