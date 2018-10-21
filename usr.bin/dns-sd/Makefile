# $MidnightBSD$

NO_MAN=	

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/mDNSResponder/Clients

PROG=   dns-sd

WARNS=	2

SRCS=	dns-sd.c ClientCommon.c

SRCDIRS= ${.CURDIR}/../../contrib/mDNSResponder/mDNSShared \
	${.CURDIR}/../../contrib/mDNSResponder/Clients

CFLAGS+= -I. -I${.CURDIR}/../../contrib/mDNSResponder/Clients
CFLAGS+= -I${.CURDIR}/../../contrib/mDNSResponder/mDNSShared
CFLAGS+= -ITARGET_OS_FREEBSD

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DHAVE_IPV6
.endif

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread -ldns_sd

.include <bsd.prog.mk>

