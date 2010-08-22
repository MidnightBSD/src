# $MidnightBSD$

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix

PROG=   mdnsd
#SRCS=   mDNSPosix/PosixDaemon.c mDNSPosix/mDNSPosix.c \
#	mDNSPosix/mDNSUNP.c \
#	mDNSCore/mDNS.c mDNSCore/DNSDigest.c \
#	mDNSCore/uDNS.c mDNSCore/DNSCommon.c \
#	mDNSShared/uds_daemon.c mDNSShared/mDNSDebug.c \
#	mDNSShared/dnssd_ipc.c \
#	mDNSShared/GenLinkedList.c mDNSShared/PlatformCommon.c

SRCS=	PosixDaemon.c mDNSPosix.c mDNSUNP.c 
.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
SRCS+=	mDNS.c DNSDigest.c uDNS.c DNSCommon.c
.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
SRCS+=	 uds_daemon.c \
	mDNSDebug.c dnssd_ipc.c GenLinkedList.c \
	PlatformCommon.c

SRCDIRS= ${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
MAN=    mDNS.1 mDNSResponder.8

CFLAGS+= -I. -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared

.include <bsd.prog.mk>

