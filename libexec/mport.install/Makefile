PROG= mport.install

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.install
 
CFLAGS+=       -I${MPORTDIR}/libmport -fblocks

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
