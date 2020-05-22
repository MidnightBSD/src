.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/Clients

PROG=   dns-sd

WARNS=	2
MK_MAN=	no

SRCS=	dns-sd.c ClientCommon.c

SRCDIRS= ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../contrib/mDNSResponder/Clients

CFLAGS+= -I. -I${.CURDIR}/../../contrib/mDNSResponder/Clients
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -ITARGET_OS_FREEBSD

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DHAVE_IPV6
.endif

LIBADD+=	dns_sd pthread

.include <bsd.prog.mk>

