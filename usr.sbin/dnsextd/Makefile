# $MidnightBSD$

WARNS=0
.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix

PROG=   dnsextd

SRCS=	mDNSPosix.c mDNSUNP.c 
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
SRCS+=	DNSDigest.c DNSCommon.c
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
SRCS+=	dnsextd.c mDNSDebug.c GenLinkedList.c \
	PlatformCommon.c dnsextd_parser.y y.tab.h dnsextd_parser.h dnsextd_lexer.l \
	CryptoAlg.c

SRCDIRS= ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
MAN=	dnsextd.8

CFLAGS+= -I. -I${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -DTARGET_OS_FREEBSD -DMDNS_VERSIONSTR_NODTS -D_REENTRANT

.if ${MK_INET6_SUPPORT} == "no"
CFLAGS+=	-DHAVE_IPV6=0
.else
CFLAGS+=	-DHAVE_IPV6=1
.endif

YFLAGS+=-v
CLEANFILES= y.output dnsextd_parser.h

DPADD=	${LIBL} ${LIBPTHREAD}
LDADD=	-ll -lpthread

dnsextd_parser.h:
	mv y.tab.h ${.TARGET}


.include <bsd.prog.mk>

