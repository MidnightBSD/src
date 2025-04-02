.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared

PROG=   mDNSResponderPosix
MAN8=	mDNSResponderPosix.8

WARNS=	1

SRCS=	Responder.c mDNSPosix.c mDNSUNP.c 
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
SRCS+=	mDNS.c DNSDigest.c uDNS.c DNSCommon.c
SRCS+=	mDNSDebug.c dnssd_ipc.c GenLinkedList.c \
	PlatformCommon.c CryptoAlg.c

SRCDIRS= ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSCore

CFLAGS+= -I. -I${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -DTARGET_OS_FREEBSD -DMDNS_VERSIONSTR_NODTS
CFLAGS+= -DIDLESLEEPCONTROL_DISABLED
CFLAGS+= -D_REENTRANT -DMDNS_DEBUGMSGS=0

# Dual stack freaks out mdnsd and it dies
.if ${MK_INET6_SUPPORT} == "no"
CFLAGS+=	-DHAVE_IPV6=0
.else
CFLAGS+=	-DHAVE_IPV6=1
.endif

.include <bsd.prog.mk>

