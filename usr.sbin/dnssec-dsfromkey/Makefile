# $MidnightBSD: src/usr.sbin/dnssec-dsfromkey/Makefile,v 1.1 2010/01/16 17:09:56 laffer1 Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind96
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-dsfromkey

.PATH: ${SRCDIR}
SRCS+=		dnssec-dsfromkey.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

WARNS?=		3

MAN=		dnssec-dsfromkey.8

.include <bsd.prog.mk>
