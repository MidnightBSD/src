# $MidnightBSD: src/apache/lib/libdispatch/Makefile,v 1.3 2010/08/14 22:15:14 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SRCDIR=	${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SHLIB_MAJOR=	1

LIB=		dns_sd
SRCS=	dnssd_clientlib.c dnssd_clientstub.c dnssd_ipc.c

WARNS?= 0

#.PATH:  ${.CURDIR}/../../../contrib/libdispatch/dispatch
#INCS=	base.h dispatch.h group.h object.h once.h \
#	queue.h semaphore.h source.h time.h
#INCSDIR=	${INCLUDEDIR}/dispatch

#if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=	-march=i586
#end
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../../contrib/mDNSResponder/mDNShared

NO_MAN=

.include <bsd.lib.mk>
