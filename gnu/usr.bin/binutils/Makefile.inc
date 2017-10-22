# $FreeBSD: release/7.0.0/gnu/usr.bin/binutils/Makefile.inc 162256 2006-09-12 19:24:01Z obrien $

.if (${TARGET_ARCH} == "alpha")
WARNS?=	2
.else
WARNS?=	3
.endif

.include "../Makefile.inc"
