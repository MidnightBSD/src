# $FreeBSD: stable/11/libexec/rtld-elf/i386/Makefile.inc 286317 2015-08-05 12:53:55Z vangyzen $

CFLAGS+=	${CFLAGS_NO_SIMD} -msoft-float
# Uncomment this to build the dynamic linker as an executable instead
# of a shared library:
#LDSCRIPT=	${.CURDIR}/${MACHINE_CPUARCH}/elf_rtld.x
