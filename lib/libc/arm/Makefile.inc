# $FreeBSD: stable/9/lib/libc/arm/Makefile.inc 193145 2009-05-31 02:03:40Z marcel $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map
