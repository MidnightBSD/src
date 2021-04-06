PROG= mport.updepends

PROG= mport.list

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/${PROG}

CFLAGS+=       -I${MPORTDIR}/libmport 

MK_MAN= no

LIBADD= mport sqlite3  pthread

.include <bsd.prog.mk>
