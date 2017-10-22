# $FreeBSD: release/7.0.0/lib/libkse/arch/powerpc/Makefile.inc 172491 2007-10-09 13:42:34Z obrien $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c
