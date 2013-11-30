# $MidnightBSD: src/apache/lib/libdns_sd/Makefile,v 1.2 2010/08/22 20:18:58 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SRCDIR=	${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SHLIB_MAJOR=	1

LIB=		dns_sd
SRCS=	dnssd_clientlib.c dnssd_clientstub.c dnssd_ipc.c

WARNS?= 0

INCS=	dns_sd.h

.if ${MACHINE_ARCH}x == "amd64x"
CFLAGS+=        -march=athlon64
.endif
.if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=        -march=i586
.endif
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder/mDNShared

NO_MAN=

.include <bsd.lib.mk>
