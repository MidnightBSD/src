# $MidnightBSD: src/usr.sbin/dnssec-keyfromlabel/Makefile,v 1.2 2013/01/01 23:18:01 laffer1 Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind98
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-keyfromlabel

.PATH: ${SRCDIR}
SRCS+=		dnssec-keyfromlabel.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

WARNS?=		3

MAN=		dnssec-keyfromlabel.8

.include <bsd.prog.mk>
