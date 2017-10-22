# $FreeBSD: release/7.0.0/lib/libc/i386/Makefile.inc 156613 2006-03-13 01:15:01Z deischen $

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/i386/Symbol.map
