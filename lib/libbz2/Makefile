# $MidnightBSD$
# $FreeBSD: stable/10/lib/libbz2/Makefile 313591 2017-02-11 06:19:25Z ngie $

BZ2DIR=	${SRCTOP}/contrib/bzip2
.PATH: ${BZ2DIR}

LIB=		bz2
SHLIB_MAJOR=	4
SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
INCS=		bzlib.h
CFLAGS+=	-I${BZ2DIR}

WARNS?=		3

.include <bsd.lib.mk>
