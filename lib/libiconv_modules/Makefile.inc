# $FreeBSD: stable/9/lib/libiconv_modules/Makefile.inc 219019 2011-02-25 00:04:39Z gabor $

.PATH: ${.CURDIR}/../../libc/iconv

SHLIB_MAJOR= 4
WARNS?=	6
CFLAGS+= -I${.CURDIR}/../../libc/iconv
.if !defined(COMPAT_32BIT)
SHLIBDIR= /usr/lib/i18n
.else
SHLIBDIR= /usr/lib32/i18n
.endif
LIBDIR=	 ${SHLIBDIR}
NO_PROFILE=
