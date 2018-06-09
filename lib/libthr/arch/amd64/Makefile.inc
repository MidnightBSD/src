# $MidnightBSD$
#$FreeBSD: stable/10/lib/libthr/arch/amd64/Makefile.inc 290014 2015-10-26 16:21:56Z vangyzen $

SRCS+=	pthread_md.c _umtx_op_err.S

# With the current compiler and libthr code, using SSE in libthr
# does not provide enough performance improvement to outweigh
# the extra context switch cost.  This can measurably impact
# performance when the application also does not use enough SSE.
CFLAGS+=${CFLAGS_NO_SIMD}
