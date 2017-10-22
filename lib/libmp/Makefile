# $FreeBSD: release/7.0.0/lib/libmp/Makefile 169807 2007-05-21 02:49:08Z deischen $

LIB=		mp
SHLIB_MAJOR=	6
DPADD=		${LIBCRYPTO}
LDADD=		-lcrypto
MAN=		libmp.3
INCS=		mp.h
SRCS=		mpasbn.c

WARNS?=		0
CFLAGS+=	-I${.CURDIR}/../../crypto

.include <bsd.lib.mk>
