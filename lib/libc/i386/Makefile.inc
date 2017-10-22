# $FreeBSD: release/10.0.0/lib/libc/i386/Makefile.inc 174204 2007-12-03 07:17:33Z das $

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
