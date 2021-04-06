PROG= mport.init

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.init
 
CFLAGS+=       -I${MPORTDIR}/libmport 

MK_MAN=		no

LIBADD+=	mport sqlite3  pthread


.include <bsd.prog.mk>
