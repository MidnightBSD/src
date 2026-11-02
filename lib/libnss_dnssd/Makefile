PACKAGE=	lib${LIB}

.include <src.opts.mk>

LIB=	nss_dnssd
SHLIB_MAJOR=	1
SHLIB_NAME=	${LIB}.so.${SHLIB_MAJOR}
SRCS=	nss_dnssd.c
MK_INSTALLLIB=	no
CFLAGS+=	-I${SRCTOP}/contrib/mDNSResponder/mDNSShared
LIBADD=	dns_sd
MAN=	nss_dnssd.8

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
