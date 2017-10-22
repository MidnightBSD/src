# $FreeBSD: release/7.0.0/lib/libc/ia64/Makefile.inc 156770 2006-03-16 14:27:17Z ru $
#
# Machine dependent definitions for the ia64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/ia64/Symbol.map
