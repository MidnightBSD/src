# $FreeBSD: release/7.0.0/lib/libkse/arch/sparc64/Makefile.inc 172491 2007-10-09 13:42:34Z obrien $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S
