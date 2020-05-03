# $FreeBSD: stable/11/lib/libiconv_modules/mapper_parallel/Makefile 313538 2017-02-10 07:32:40Z ngie $

.PATH: ${.CURDIR:H}/mapper_serial

SHLIB=	mapper_parallel
SRCS+=	citrus_mapper_serial.c
CFLAGS.gcc+= --param max-inline-insns-single=32

.include <bsd.lib.mk>
