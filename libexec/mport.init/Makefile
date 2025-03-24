PROG= mport.init

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.init
 
CFLAGS+=       -I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/

ZSTDDIR=        ${SRCTOP}/sys/contrib/zstd
CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib

MK_MAN=		no

LIBADD+=	mport sqlite3  pthread


.include <bsd.prog.mk>
