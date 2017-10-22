# $FreeBSD: stable/9/usr.sbin/dnssec-revoke/Makefile 224093 2011-07-16 11:20:54Z dougb $

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-revoke

.PATH: ${SRCDIR}
SRCS+=		dnssec-revoke.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

WARNS?=		3

MAN=		dnssec-revoke.8

.include <bsd.prog.mk>
