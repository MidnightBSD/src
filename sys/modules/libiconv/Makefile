# $FreeBSD: stable/11/sys/modules/libiconv/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/libkern ${SRCTOP}/sys/sys

KMOD=	libiconv
SRCS=	iconv.c iconv_ucs.c iconv_xlat.c iconv_xlat16.c
SRCS+=	iconv.h
SRCS+=	iconv_converter_if.c iconv_converter_if.h

EXPORT_SYMS=	iconv_add	\
		iconv_open	\
		iconv_close	\
		iconv_conv	\
		iconv_conv_case	\
		iconv_convchr	\
		iconv_convchr_case	\
		iconv_convstr	\
		iconv_convmem	\
		iconv_vfs_refcount

.include <bsd.kmod.mk>
