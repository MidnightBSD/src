# $FreeBSD: stable/11/lib/libc/sparc64/Makefile.inc 315044 2017-03-11 02:51:29Z brooks $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

SRCS+=	trivial-vdso_tc.c

# Long double is quad precision
GDTOASRCS+=strtorQ.c
SRCS+=machdep_ldisQ.c
SYM_MAPS+=${LIBC_SRCTOP}/sparc64/Symbol.map
