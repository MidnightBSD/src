# $FreeBSD: release/10.0.0/lib/libiconv_modules/Makefile.inc 255298 2013-09-06 10:40:38Z theraven $

.PATH: ${.CURDIR}/../../libc/iconv

SHLIB_MAJOR= 4
WARNS?=	6
CFLAGS+= -I${.CURDIR}/../../libc/iconv

CFLAGS+=	-Dbool=_Bool

.if !defined(COMPAT_32BIT)
SHLIBDIR= /usr/lib/i18n
.else
SHLIBDIR= /usr/lib32/i18n
.endif
LIBDIR=	 ${SHLIBDIR}
NO_PROFILE=
