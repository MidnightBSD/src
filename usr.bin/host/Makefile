# $MidnightBSD: src/usr.bin/host/Makefile,v 1.4 2012/11/23 01:13:51 laffer1 Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind98
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dig

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		host

.PATH: ${SRCDIR}
SRCS+=		dighost.c host.c

CFLAGS+=	-I${SRCDIR}/include
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include
WARNS?=		1
DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

.include <bsd.prog.mk>
