WARNS=0
.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix

PROG=   dnsextd

SRCS=	mDNSPosix.c mDNSUNP.c mDNSDebug.c GenLinkedList.c DNSDigest.c \
	DNSCommon.c PlatformCommon.c dnsextd_parser.y dnsextd_lexer.l CryptoAlg.c  \
	dnsextd.c

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared

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

LIBADD+=	l pthread

.include <bsd.prog.mk>

