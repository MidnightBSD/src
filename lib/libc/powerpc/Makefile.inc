# $FreeBSD: stable/9/lib/libc/powerpc/Makefile.inc 246775 2013-02-13 23:25:11Z kib $

SRCS+=	__vdso_gettc.c

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
