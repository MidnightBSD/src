PROG= mport.fetch

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.fetch
 
CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/
CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include

ZSTDDIR=        ${SRCTOP}/sys/contrib/zstd
CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib

MK_MAN= no

LIBADD= mport ucl pthread

.include <bsd.prog.mk>
