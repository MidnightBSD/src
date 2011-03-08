# $MidnightBSD: src/apache/lib/libdns_sd/Makefile,v 1.3 2010/08/30 17:10:34 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
SRCDIR=	${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
SHLIB_MAJOR=	0

LIB=	nss_mdns
SRCS=	nss_mdns.c netbsd.c

WARNS?= 0

#INCS=	netbsd.h

.if ${MACHINE_ARCH}x == "amd64x"
CFLAGS+=        -march=athlon64
.endif
.if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=        -march=i586
.endif
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix

LDADD=	-ldns_sd

MAN= nss_mdns.conf.5

.include <bsd.lib.mk>
