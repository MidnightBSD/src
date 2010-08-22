# $MidnightBSD: src/apache/usr.sbin/dnsextd/Makefile,v 1.2 2010/08/22 21:03:02 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/Clients

PROG=   dns-sd

SRCS=	dns-sd.c ClientCommon.c

SRCDIRS= ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../../contrib/mDNSResponder/Clients
NO_MAN=

CFLAGS+= -I. -I${.CURDIR}/../../../contrib/mDNSResponder/Clients
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -ITARGET_OS_FREEBSD

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread -ldns_sd

.include <bsd.prog.mk>

