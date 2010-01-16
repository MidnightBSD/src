# $MidnightBSD: src/usr.sbin/dnssec-keygen/Makefile,v 1.2 2008/04/28 03:28:26 laffer1 Exp $
# $FreeBSD: src/usr.sbin/dnssec-keygen/Makefile,v 1.4 2007/06/02 23:19:57 dougb Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind96
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-keygen

.PATH: ${SRCDIR}
SRCS+=		dnssec-keygen.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MAN=		dnssec-keygen.8

.include <bsd.prog.mk>
