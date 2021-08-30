PROG= mport.query

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/${PROG}

CFLAGS+=       -I${MPORTDIR}/libmport 

MK_MAN= no

LIBADD= mport  pthread sqlite3

.include <bsd.prog.mk>
