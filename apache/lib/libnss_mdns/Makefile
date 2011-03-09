# $MidnightBSD: src/apache/lib/libnss_mdns/Makefile,v 1.1 2011/03/08 22:13:48 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
SRCDIR=	${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
SHLIB_MAJOR=	1

LIB=	nss_mdns
SRCS=	nss_mdns.c netbsd.c

WARNS?= 0

.if ${MACHINE_ARCH}x == "amd64x"
CFLAGS+=        -march=athlon64
.endif
.if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=        -march=i586
.endif
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix

DPADD=	${LIBDNS_SD}
LDADD=	-ldns_sd

MAN= nss_mdns.conf.5

.include <bsd.lib.mk>
