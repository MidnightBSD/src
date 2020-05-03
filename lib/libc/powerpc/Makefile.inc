# $FreeBSD: stable/11/lib/libc/powerpc/Makefile.inc 315044 2017-03-11 02:51:29Z brooks $

SRCS+=	trivial-vdso_tc.c

# Long double is 64-bits
SRCS+=machdep_ldisd.c
SYM_MAPS+=${LIBC_SRCTOP}/powerpc/Symbol.map
