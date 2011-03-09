# $MidnightBSD: src/apache/lib/libnss_mdns/Makefile,v 1.3 2011/03/09 20:19:01 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
SRCDIR=	${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
SHLIB_MAJOR=	1

LIB=	nss_mdns
SRCS=	nss_mdns.c netbsd.c
SHLIB_NAME?=    ${LIB}.so.${SHLIB_MAJOR}

NO_PROFILE=
NO_INSTALLLIB=

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

MAN=	nss_mdns.conf.5 \
	libnss_mdns.8

.include <bsd.lib.mk>
