PROG= mport.update

PROG= mport.list

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/${PROG}

CFLAGS+=       -I${MPORTDIR}/libmport -fblocks

MK_MAN= no

LIBADD= mport sqlite3 dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
