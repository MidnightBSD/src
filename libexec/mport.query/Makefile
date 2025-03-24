PROG= mport.query

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/${PROG}

CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/

CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include
CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib

MK_MAN= no

LIBADD= mport  pthread sqlite3

.include <bsd.prog.mk>
