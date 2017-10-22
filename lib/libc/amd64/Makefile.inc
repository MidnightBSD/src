# $FreeBSD: release/7.0.0/lib/libc/amd64/Makefile.inc 156769 2006-03-16 14:22:19Z ru $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/amd64/Symbol.map
