PROG= mport.check-fake

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libexec/mport.check-fake

CFLAGS+=	-I${MPORTDIR}/libmport -fblocks

MK_MAN=	no

LIBADD=	mport sqlite3 dispatch BlocksRuntime pthread

.include <bsd.prog.mk>
