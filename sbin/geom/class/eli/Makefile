# $MidnightBSD$
# $FreeBSD: stable/10/sbin/geom/class/eli/Makefile 314327 2017-02-27 08:27:38Z avg $

.PATH: ${.CURDIR}/../../misc ${.CURDIR}/../../../../sys/geom/eli ${.CURDIR}/../../../../sys/crypto/sha2

GEOM_CLASS=	eli
SRCS=	g_eli_crypto.c
SRCS+=	g_eli_key.c
SRCS+=	pkcs5v2.c
SRCS+=	sha256c.c
SRCS+=	sha512c.c

DPADD=	${LIBMD} ${LIBCRYPTO}
LDADD=	-lmd -lcrypto

WARNS?=	3

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
