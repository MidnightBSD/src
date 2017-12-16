# $MidnightBSD$
# $FreeBSD: stable/10/lib/libiconv_modules/mapper_parallel/Makefile 313626 2017-02-11 07:02:04Z ngie $

.PATH: ${.CURDIR:H}/mapper_serial

SHLIB=	mapper_parallel
SRCS+=	citrus_mapper_serial.c
CFLAGS.gcc+= --param max-inline-insns-single=32

.include <bsd.lib.mk>
