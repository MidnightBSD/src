# $FreeBSD: release/7.0.0/lib/libc/arm/Makefile.inc 156613 2006-03-13 01:15:01Z deischen $
#
# Machine dependent definitions for the arm architecture.
#

SOFTFLOAT_BITS=32

CFLAGS+=-DSOFTFLOAT

# Long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/arm/Symbol.map
