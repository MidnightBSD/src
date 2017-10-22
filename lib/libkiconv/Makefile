# $FreeBSD: release/7.0.0/lib/libkiconv/Makefile 169807 2007-05-21 02:49:08Z deischen $

LIB=		kiconv
SHLIBDIR?=	/lib
SRCS=		xlat16_iconv.c xlat16_sysctl.c
SRCS+=		quirks.c

SHLIB_MAJOR=	3

MAN=		kiconv.3

MLINKS+=	kiconv.3 kiconv_add_xlat16_cspair.3 \
		kiconv.3 kiconv_add_xlat16_cspairs.3 \
		kiconv.3 kiconv_add_xlat16_table.3

CFLAGS+=	-I${.CURDIR}/../../sys

.include <bsd.lib.mk>
