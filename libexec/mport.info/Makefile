PROG=	mport.info

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.info
 
CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/

ZSTDDIR=        ${SRCTOP}/sys/contrib/zstd
CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib

MK_MAN= no

LIBADD= mport  pthread

CFLAGS+=        -I${SRCTOP}/lib/libmport -I${OBJTOP}/lib/libmport

.include <bsd.prog.mk>
