# $MidnightBSD: src/usr.sbin/named-checkzone/Makefile,v 1.3 2010/01/16 17:00:07 laffer1 Exp $
# $FreeBSD: src/usr.sbin/named-checkzone/Makefile,v 1.5 2007/06/02 23:19:58 dougb Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind98
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/check

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		named-checkzone

.PATH: ${SRCDIR}
SRCS+=		named-checkzone.c check-tool.c

CFLAGS+=	-I${LIB_BIND_DIR}

WARNS?=		3

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

LINKS=		${BINDIR}/named-checkzone ${BINDIR}/named-compilezone

MAN=		named-checkzone.8

MLINKS=		named-checkzone.8 named-compilezone.8

.include <bsd.prog.mk>
