# $FreeBSD: release/7.0.0/lib/libc/sparc64/Makefile.inc 156769 2006-03-16 14:22:19Z ru $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

# Long double is quad precision
GDTOASRCS+=strtopQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
