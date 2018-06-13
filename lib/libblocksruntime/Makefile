# $MidnightBSD$
# $FreeBSD: stable/10/lib/libblocksruntime/Makefile 313586 2017-02-11 05:56:48Z ngie $

LIB=	BlocksRuntime
SHLIB_MAJOR=0
CFLAGS+=-I${.CURDIR}
WARNS?=	2

.PATH: ${SRCTOP}/contrib/compiler-rt/BlocksRuntime

INCS=	Block.h Block_private.h
SRCS=	data.c runtime.c

.include <bsd.lib.mk>
