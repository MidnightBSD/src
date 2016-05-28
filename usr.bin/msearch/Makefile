# $MidnightBSD$

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

CSTD=	c99

DPADD=  ${LIBMSEARCH} ${LIBZ} ${LIBSQLITE3}
LDADD=  -lmsearch -lz -lsqlite3

.include <bsd.prog.mk>
