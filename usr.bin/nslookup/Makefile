# $MidnightBSD: src/usr.bin/nslookup/Makefile,v 1.2 2008/04/28 03:20:26 laffer1 Exp $
# $FreeBSD: src/usr.bin/nslookup/Makefile,v 1.7 2007/06/02 23:19:57 dougb Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind96
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dig

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		nslookup

.PATH: ${SRCDIR}
SRCS+=		dighost.c nslookup.c

CFLAGS+=	-I${SRCDIR}/include
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MANFILTER=	sed -e "s@^host \[server\]@\\\fBhost\\\fR \\\fI[server]\\\fR@"

.include <bsd.prog.mk>
