# $FreeBSD: release/10.0.0/usr.bin/mkesdb_static/Makefile 219019 2011-02-25 00:04:39Z gabor $

.PATH: ${.CURDIR}/../../lib/libc/iconv ${.CURDIR}/../mkesdb

PROG=	mkesdb_static
SRCS=	citrus_bcs.c citrus_db_factory.c citrus_db_hash.c \
	citrus_lookup_factory.c
NO_MAN=	yes
NO_SHARED= yes

build-tools: mkesdb_static

.include "${.CURDIR}/../mkesdb/Makefile.inc"
.include <bsd.prog.mk>
