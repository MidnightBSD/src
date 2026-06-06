PROG= mport.create

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.create
 
CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/ \
		-I${SRCTOP}/contrib/libucl/include

WARNS?= 	6

MK_MAN= no

LIBADD= mport ucl pthread

.include <bsd.prog.mk>
