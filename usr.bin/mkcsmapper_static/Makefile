# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/mkcsmapper_static/Makefile 296740 2016-03-12 18:54:02Z bdrewery $

.PATH: ${.CURDIR}/../../lib/libc/iconv ${.CURDIR}/../mkcsmapper

PROG=	mkcsmapper_static
SRCS=	citrus_bcs.c citrus_db_factory.c citrus_db_hash.c \
	citrus_lookup_factory.c citrus_pivot_factory.c
MAN=
NO_SHARED= yes
NO_WMISSING_VARIABLE_DECLARATIONS=

build-tools: mkcsmapper_static

.include "${.CURDIR}/../mkcsmapper/Makefile.inc"
.include <bsd.prog.mk>
