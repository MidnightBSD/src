PROG= mport

MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/mport

CFLAGS= -I${MPORTDIR}/libmport/

CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include

LIBADD+=	mport ucl util

.include <bsd.prog.mk>
