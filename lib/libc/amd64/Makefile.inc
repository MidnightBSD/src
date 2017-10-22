# $FreeBSD: release/10.0.0/lib/libc/amd64/Makefile.inc 174204 2007-12-03 07:17:33Z das $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/amd64/Symbol.map
