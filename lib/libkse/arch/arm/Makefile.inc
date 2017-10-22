# $FreeBSD: release/7.0.0/lib/libkse/arch/arm/Makefile.inc 172491 2007-10-09 13:42:34Z obrien $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S
