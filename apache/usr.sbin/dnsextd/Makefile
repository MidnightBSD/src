# $MidnightBSD$

WARNS=0
.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix

PROG=   dnsextd

SRCS=	mDNSPosix.c mDNSUNP.c 
.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
SRCS+=	DNSDigest.c DNSCommon.c
.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SRCS+=	dnsextd.c mDNSDebug.c GenLinkedList.c \
	PlatformCommon.c dnsextd_parser.y dnsextd_lexer.l \
	CryptoAlg.c \
	y.tab.h

SRCDIRS= ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
MAN=	dnsextd.8

CFLAGS+= -I. -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -DTARGET_OS_FREEBSD -DMDNS_VERSIONSTR_NODTS

.if ${MK_INET6_SUPPORT} == "no"
CFLAGS+=	-DHAVE_IPV6=0
.else
CFLAGS+=	-DHAVE_IPV6=1
.endif

YFLAGS+=-v
CLEANFILES= y.output

DPADD=	${LIBL} ${LIBPTHREAD}
LDADD=	-ll -lpthread

.include <bsd.prog.mk>

