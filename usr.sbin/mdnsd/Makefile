.include <src.opts.mk>

.PATH: ${SRCTOP}/contrib/mDNSResponder/mDNSPosix

PROG=   mdnsd

WARNS=	2

DAEMONOBJS = $(OBJDIR)/PosixDaemon.c.o $(OBJDIR)/mDNSPosix.c.o $(OBJDIR)/mDNSUNP.c.o $(OBJDIR)/mDNS.c.o \
             $(OBJDIR)/DNSDigest.c.o $(OBJDIR)/uDNS.c.o $(OBJDIR)/DNSCommon.c.o $(OBJDIR)/uds_daemon.c.o \
             $(OBJDIR)/mDNSDebug.c.o $(OBJDIR)/dnssd_ipc.c.o $(OBJDIR)/GenLinkedList.c.o \
             $(OBJDIR)/PlatformCommon.c.o $(OBJDIR)/CryptoAlg.c.o $(OBJDIR)/ClientRequests.c.o \
             $(OBJDIR)/dso.c.o $(OBJDIR)/dso-transport.c.o $(OBJDIR)/dnssd_clientshim.c.o \
             $(OBJDIR)/posix_utilities.c.o


SRCS=	PosixDaemon.c mDNSPosix.c mDNSUNP.c 
.PATH: ${SRCTOP}/contrib/mDNSResponder/mDNSCore
SRCS+=	mDNS.c DNSDigest.c uDNS.c DNSCommon.c
.PATH: ${SRCTOP}/contrib/mDNSResponder/mDNSShared
SRCS+=	 uds_daemon.c \
	mDNSDebug.c dnssd_ipc.c GenLinkedList.c \
	PlatformCommon.c CryptoAlg.c ClientRequests.c
.PATH: ${SRCTOP}/contrib/mDNSResponder/DSO
SRCS+=	dso.c dso-transport.c dnssd_clientshim.c posix_utilities.c

SRCDIRS= ${SRCTOP}/contrib/mDNSResponder/mDNSShared \
	${SRCTOP}/contrib/mDNSResponder/mDNSPosix \
	${SRCTOP}/contrib/mDNSResponder/mDNSCore \
	${SRCTOP}/contrib/mDNSResponder/DSO
MAN=    mDNSResponderPosix.8
MLINKS+= mDNSResponderPosix.8 mDNSResponder.8

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

