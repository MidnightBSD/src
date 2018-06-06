# $MidnightBSD$
# $FreeBSD: stable/10/lib/libucl/Makefile 268896 2014-07-19 23:44:57Z bapt $

LIBUCL=	${.CURDIR}/../../contrib/libucl

LIB=		ucl
PRIVATELIB=	true
SHLIB_MAJOR=	1
SRCS=		ucl_emitter.c \
		ucl_hash.c \
		ucl_parser.c  \
		ucl_schema.c \
		ucl_util.c \
		xxhash.c

.PATH:		${LIBUCL}/src

WARNS=		1
CFLAGS+=	-I${LIBUCL}/include \
		-I${LIBUCL}/src \
		-I${LIBUCL}/uthash

.include <bsd.lib.mk>
