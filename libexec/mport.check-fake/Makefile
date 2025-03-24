PROG= mport.check-fake

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.check-fake

CFLAGS+=	-I${MPORTDIR}/libmport  -I${MPORTDIR}/external/tllist/

MK_MAN=	no

LIBADD=	mport sqlite3  pthread

.include <bsd.prog.mk>
