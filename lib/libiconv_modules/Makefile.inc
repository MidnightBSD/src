# $MidnightBSD$
# $FreeBSD: stable/10/lib/libiconv_modules/Makefile.inc 313626 2017-02-11 07:02:04Z ngie $

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
NO_PROFILE=
