# $FreeBSD: stable/9/gnu/usr.bin/cc/libdecnumber/Makefile 169718 2007-05-19 04:25:59Z kan $

# Use our headers in preference to ones from ../cc_tools.
CFLAGS+=	-I${.CURDIR} -I.

.include "../Makefile.inc"

.PATH: ${GCCLIB}/libdecnumber

LIB=	decnumber
SRCS=	decNumber.c decContext.c decUtility.c \
        decimal32.c decimal64.c decimal128.c

INTERNALLIB=
WARNS?=	1

.include <bsd.lib.mk>
