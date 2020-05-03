# $FreeBSD: stable/11/lib/libiconv_modules/Makefile.inc 313538 2017-02-10 07:32:40Z ngie $

.PATH: ${SRCTOP}/lib/libc/iconv

SHLIB_MAJOR= 4
WARNS?=	6
CFLAGS+= -I${SRCTOP}/lib/libc/iconv

CFLAGS+=	-Dbool=_Bool

.if !defined(COMPAT_32BIT)
SHLIBDIR= /usr/lib/i18n
.else
SHLIBDIR= /usr/lib32/i18n
.endif
LIBDIR=	 ${SHLIBDIR}
MK_PROFILE=	no
