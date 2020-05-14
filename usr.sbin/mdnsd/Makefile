.include <src.opts.mk>

.PATH: ${SRCTOP}/contrib/mDNSResponder/mDNSPosix

PROG=   mdnsd

WARNS=	2

SRCS=	PosixDaemon.c mDNSPosix.c mDNSUNP.c 
.PATH: ${SRCTOP}/contrib/mDNSResponder/mDNSCore
SRCS+=	mDNS.c DNSDigest.c uDNS.c DNSCommon.c
.PATH: ${SRCTOP}/contrib/mDNSResponder/mDNSShared
SRCS+=	 uds_daemon.c \
	mDNSDebug.c dnssd_ipc.c GenLinkedList.c \
	PlatformCommon.c CryptoAlg.c anonymous.c

SRCDIRS= ${SRCTOP}/contrib/mDNSResponder/mDNSShared \
	${SRCTOP}/contrib/mDNSResponder/mDNSPosix \
	${SRCTOP}/contrib/mDNSResponder/mDNSCore
MAN=    mDNSResponder.8

CFLAGS+= -I. -I${SRCTOP}/contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${SRCTOP}/contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${SRCTOP}/contrib/mDNSResponder/mDNSShared
CFLAGS+= -DTARGET_OS_FREEBSD -DMDNS_VERSIONSTR_NODTS
CFLAGS+= -DIDLESLEEPCONTROL_DISABLED
CFLAGS+= -D_REENTRANT

# Dual stack freaks out mdnsd and it dies
.if ${MK_INET6_SUPPORT} == "no"
CFLAGS+=	-DHAVE_IPV6=0
.else
CFLAGS+=	-DHAVE_IPV6=1
.endif

.include <bsd.prog.mk>

