.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix

PROG=   mDNSProxyResponderPosix

WARNS=	1

MK_MAN=	no

SRCS=	ProxyResponder.c mDNSPosix.c mDNSUNP.c 
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
SRCS+=	mDNS.c DNSDigest.c uDNS.c DNSCommon.c
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
SRCS+=	mDNSDebug.c dnssd_ipc.c GenLinkedList.c \
	PlatformCommon.c
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/DSO
SRCS+=	dso.c dso-transport.c dnssd_clientshim.c
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared/utilities
SRCS+=	misc_utilities.c

SRCDIRS= ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSCore

CFLAGS+= -I. -I${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSShared/utilities
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/ServiceRegistration
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

