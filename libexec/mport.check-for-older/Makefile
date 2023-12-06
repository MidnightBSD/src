PROG= mport.check-for-older

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.check-for-older

CFLAGS+=       -I${MPORTDIR}/libmport 
CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include

WARNS?= 	4

MK_MAN= no

LIBADD= mport ucl pthread

.include <bsd.prog.mk>
