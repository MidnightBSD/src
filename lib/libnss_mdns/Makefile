.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix
SRCDIR=	${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix \
	${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
SHLIB_MAJOR=	1

LIB=	nss_mdns
SRCS=	nss_mdns.c netbsd.c
.PATH: ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
SRCS+=	dnssd_clientlib.c dnssd_clientstub.c dnssd_ipc.c
SHLIB_NAME?=    ${LIB}.so.${SHLIB_MAJOR}

NO_PROFILE=
NO_INSTALLLIB=

WARNS?= 0

.if ${MACHINE_ARCH}x == "amd64x"
CFLAGS+=        -march=athlon64
.endif
.if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=        -march=i586
.endif
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../contrib/mDNSResponder
CFLAGS+=	-I${.CURDIR}/../../contrib/mDNSResponder/mDNSPosix
CFLAGS+=        -I${.CURDIR}/../../contrib/mDNSResponder/mDNShared
CFLAGS+=        -DTARGET_OS_FREEBSD

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DHAVE_IPV6
.endif

MAN=	nss_mdns.conf.5 \
	libnss_mdns.8

.include <bsd.lib.mk>
