# $MidnightBSD: src/apache/usr.sbin/mdnsd/Makefile,v 1.2 2010/08/22 20:05:37 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix

PROG=   dnsextd

SRCS=	mDNSPosix.c mDNSUNP.c 
.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
SRCS+=	DNSDigest.c DNSCommon.c
.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SRCS+=	 dnsextd.c \
	mDNSDebug.c GenLinkedList.c \
	PlatformCommon.c  dnsextd_parser.y dnsextd_lexer.l

SRCDIRS= ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
#MAN=    mDNSResponder.8
NO_MAN=	

CFLAGS+= -I. -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -ITARGET_OS_FREEBSD

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

.include <bsd.prog.mk>

