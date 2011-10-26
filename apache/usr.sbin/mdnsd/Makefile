# $MidnightBSD: src/apache/usr.sbin/mdnsd/Makefile,v 1.7 2011/03/09 05:53:49 laffer1 Exp $

.include <bsd.own.mk>

MK_SSP=	no

.PATH: ${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix

PROG=   mdnsd

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
MAN=    mDNSResponder.8

CFLAGS+= -I. -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSPosix
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSCore
CFLAGS+= -I${.CURDIR}/../../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -DTARGET_OS_FREEBSD -DMDNS_VERSIONSTR_NODTS
CFLAGS+= -DIDLESLEEPCONTROL_DISABLED

# Dual stack freaks out mdnsd and it dies
#.if ${MK_INET6_SUPPORT} == "no"
CFLAGS+=	-DHAVE_IPV6=0
#.else
#CFLAGS+=	-DHAVE_IPV6=1
#.endif

.include <bsd.prog.mk>

