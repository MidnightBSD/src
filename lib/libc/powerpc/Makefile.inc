# $FreeBSD: release/10.0.0/lib/libc/powerpc/Makefile.inc 246117 2013-01-30 12:48:16Z kib $

SRCS+=	__vdso_gettc.c

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/powerpc/Symbol.map
