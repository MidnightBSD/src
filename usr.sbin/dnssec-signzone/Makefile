# $MidnightBSD: src/usr.sbin/dnssec-signzone/Makefile,v 1.3 2010/01/16 17:00:06 laffer1 Exp $
# $FreeBSD: src/usr.sbin/dnssec-signzone/Makefile,v 1.4 2007/06/02 23:19:58 dougb Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind96
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-signzone

.PATH: ${SRCDIR}
SRCS+=		dnssec-signzone.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

WARNS?=		2

MAN=		dnssec-signzone.8

.include <bsd.prog.mk>
