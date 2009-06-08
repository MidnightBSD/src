# $MidnightBSD: src/secure/usr.bin/netpgp/Makefile,v 1.1 2009/06/07 23:59:51 laffer1 Exp $

.include <bsd.own.mk>

PROG=	netpgp

NETPGP_SRC=     ${.CURDIR}/../../../crypto/netpgp

CFLAGS+=-I${NETPGP_SRC}

DPADD=  ${LIBCRYPTO} ${LIBZ} ${LIBBZ2} ${LIBNETPGP}
LDADD=  -lcrypto -lz -lbz2 -lnetpgp

MAN=	netpgp.1

.include <bsd.prog.mk>

.PATH:	${NETPGP_SRC}/src/bin
