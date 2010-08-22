# $MidnightBSD: src/apache/lib/libdns_sd/Makefile,v 1.1 2010/08/22 20:00:48 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SRCDIR=	${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SHLIB_MAJOR=	1

LIB=		dns_sd
SRCS=	dnssd_clientlib.c dnssd_clientstub.c dnssd_ipc.c

WARNS?= 0

INCS=	dns_sd.h

#if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=	-march=i586
#end
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder/mDNShared

NO_MAN=

.include <bsd.lib.mk>
