# $MidnightBSD$

.include <bsd.own.mk>

PROG=	netpgp

NETPGP_SRC=     ${.CURDIR}/../../../crypto/netpgp

CFLAGS+=-I${NETPGP_SRC}

DPADD=  ${LIBCRYPTO} ${LIBZ} ${LIBBZ2}
LDADD=  -lcrypto -lz -lbz2 -lnetpgp

MAN=	netpgp.1

.include <bsd.prog.mk>

.PATH:	${NETPGP_SRC}/src/bin
