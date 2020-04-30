# $FreeBSD: stable/11/lib/libc/riscv/Makefile.inc 315044 2017-03-11 02:51:29Z brooks $
#
# Machine dependent definitions for the RISC-V architecture.
#

# Long double is quad precision
GDTOASRCS+=strtorQ.c
SRCS+=machdep_ldisQ.c
SYM_MAPS+=${LIBC_SRCTOP}/riscv/Symbol.map
