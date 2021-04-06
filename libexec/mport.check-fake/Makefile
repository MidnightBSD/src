PROG= mport.check-fake

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.check-fake

CFLAGS+=	-I${MPORTDIR}/libmport 

MK_MAN=	no

LIBADD=	mport sqlite3  pthread

.include <bsd.prog.mk>
