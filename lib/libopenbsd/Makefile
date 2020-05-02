# $FreeBSD: stable/11/lib/libopenbsd/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=lib${LIB}
LIB=	openbsd
SRCS=	getdtablecount.c \
	imsg-buffer.c \
	imsg.c \
	ohash.c
INTERNALLIB=

CFLAGS+= -I${.CURDIR}

WARNS=	3

.include <bsd.lib.mk>
