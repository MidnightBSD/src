# $FreeBSD: stable/11/lib/libc/amd64/Makefile.inc 315044 2017-03-11 02:51:29Z brooks $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
SRCS+=machdep_ldisx.c
SYM_MAPS+=${LIBC_SRCTOP}/amd64/Symbol.map
