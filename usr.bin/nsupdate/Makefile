# $MidnightBSD: src/usr.bin/nsupdate/Makefile,v 1.4 2010/01/16 16:57:06 laffer1 Exp $
# $FreeBSD: src/usr.bin/nsupdate/Makefile,v 1.7 2007/06/02 23:19:57 dougb Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind96
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/nsupdate

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		nsupdate

.PATH: ${SRCDIR}
SRCS+=		nsupdate.c

CFLAGS+=	-I${SRCDIR}/include
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include
CFLAGS+=	-DSESSION_KEYFILE=\"/var/run/named/session.key\"

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

WARNS?=		1

MAN=		nsupdate.1

MANFILTER=	sed -e "s@/etc/named\.conf@/etc/namedb/named.conf@g" \
		-e "s@^\.HP [0-9]* @@"

.include <bsd.prog.mk>
