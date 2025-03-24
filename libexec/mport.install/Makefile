PROG= mport.install

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.install
 
CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/

ZSTDDIR=        ${SRCTOP}/sys/contrib/zstd
CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib

MK_MAN= no

LIBADD= mport  pthread

.include <bsd.prog.mk>
