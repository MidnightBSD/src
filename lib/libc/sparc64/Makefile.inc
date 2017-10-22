# $FreeBSD: stable/9/lib/libc/sparc64/Makefile.inc 246775 2013-02-13 23:25:11Z kib $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

SRCS+=	__vdso_gettc.c

# Long double is quad precision
GDTOASRCS+=strtorQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
