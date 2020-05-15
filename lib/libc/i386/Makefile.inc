# $FreeBSD: stable/11/lib/libc/i386/Makefile.inc 315044 2017-03-11 02:51:29Z brooks $

# Long double is 80 bits
GDTOASRCS+=strtorx.c
SRCS+=machdep_ldisx.c
SYM_MAPS+=${LIBC_SRCTOP}/i386/Symbol.map
