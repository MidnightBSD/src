PROG= mport

MPORTDIR=	${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/mport

ZSTDDIR=	${SRCTOP}/sys/contrib/zstd

CFLAGS= -I${MPORTDIR}/libmport/ -I${MPORTDIR}/external/tllist/

CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include

CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib

LIBADD+=md archive lzma fetch sqlite3 ucl pthread util zstd mport

.include <bsd.prog.mk>
