PROG= mport.fetch

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.fetch
 
CFLAGS+=       -I${MPORTDIR}/libmport 
CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include

MK_MAN= no

LIBADD= mport ucl pthread

.include <bsd.prog.mk>
