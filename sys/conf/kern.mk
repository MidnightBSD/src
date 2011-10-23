# $MidnightBSD$
# $FreeBSD: src/sys/conf/kern.mk,v 1.52 2007/05/24 21:53:42 obrien Exp $

#
# Warning flags for compiling the kernel and components of the kernel:
#
CWARNFLAGS?=	-Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
		-Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual \
		-Wundef -Wno-pointer-sign -fformat-extensions \
		-Wmissing-include-dirs -fdiagnostics-show-option
#
# The following flags are next up for working on:
#	-Wextra

#
# On i386, do not align the stack to 16-byte boundaries.  Otherwise GCC 2.95
# and above adds code to the entry and exit point of every function to align the
# stack to 16-byte boundaries -- thus wasting approximately 12 bytes of stack
# per function call.  While the 16-byte alignment may benefit micro benchmarks,
# it is probably an overall loss as it makes the code bigger (less efficient
# use of code cache tag lines) and uses more stack (less efficient use of data
# cache tag lines).  Explicitly prohibit the use of FPU, SSE and other SIMD
# operations inside the kernel itself.  These operations are exclusively
# reserved for user applications.
#
# gcc:
# Setting -mno-mmx implies -mno-3dnow
# Setting -mno-sse implies -mno-sse2, -mno-sse3 and -mno-ssse3
#
# clang:
# Setting -mno-mmx implies -mno-3dnow, -mno-3dnowa, -mno-sse, -mno-sse2,
#                          -mno-sse3, -mno-ssse3, -mno-sse41 and -mno-sse42
#
.if ${MACHINE_ARCH} == "i386"
.if ${CC:T:Mclang} != "clang"
CFLAGS+=	-mno-align-long-strings -mpreferred-stack-boundary=2 -mno-sse
.else
CFLAGS+=	-mno-aes -mno-avx
.endif
CFLAGS+=	-mno-mmx -msoft-float
INLINE_LIMIT?=	8000
.endif

#
# For sparc64 we want medlow code model, and we tell gcc to use floating
# point emulation.  This avoids using floating point registers for integer
# operations which it has a tendency to do.
#
.if ${MACHINE_ARCH} == "sparc64"
CFLAGS+=	-mcmodel=medany -msoft-float
INLINE_LIMIT?=	15000
.endif

#
# For AMD64, we explicitly prohibit the use of FPU, SSE and other SIMD
# operations inside the kernel itself.  These operations are exclusively
# reserved for user applications.
#
# gcc:
# Setting -mno-mmx implies -mno-3dnow
# Setting -mno-sse implies -mno-sse2, -mno-sse3, -mno-ssse3 and -mfpmath=387
#
# clang:
# Setting -mno-mmx implies -mno-3dnow, -mno-3dnowa, -mno-sse, -mno-sse2,
#                          -mno-sse3, -mno-ssse3, -mno-sse41 and -mno-sse42
# (-mfpmath= is not supported)
#
.if ${MACHINE_ARCH} == "amd64"
.if ${CC:T:Mclang} != "clang"
CFLAGS+=	-mno-sse
.else
CFLAGS+=	-mno-aes -mno-avx
.endif
CFLAGS+=	-mcmodel=kernel -mno-red-zone -mno-mmx -msoft-float \
		-fno-asynchronous-unwind-tables
INLINE_LIMIT?=	8000
.endif

#
# GCC 3.0 and above like to do certain optimizations based on the
# assumption that the program is linked against libc.  Stop this.
#
CFLAGS+=	-ffreestanding

#
# GCC SSP support
#
.if ${MK_SSP} != "no" 
CFLAGS+=	-fstack-protector
.endif
