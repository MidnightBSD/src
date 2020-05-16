.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
SRCDIR=	${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
SHLIB_MAJOR=	1

LIB=		dns_sd
SRCS=		dnssd_clientlib.c dnssd_clientstub.c dnssd_ipc.c

WARNS?= 2

INCS=	dns_sd.h

.if ${MACHINE_ARCH}x == "amd64x"
CFLAGS+=	-march=athlon64
.endif
.if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=	-march=i586
.endif
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../contrib/mDNSResponder/mDNShared
CFLAGS+=	-DTARGET_OS_FREEBSD

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DHAVE_IPV6
.endif

MK_MAN=no

LIBADD+=	pthread

.include <bsd.lib.mk>
