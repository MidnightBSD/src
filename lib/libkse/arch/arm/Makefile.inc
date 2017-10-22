# $FreeBSD: release/10.0.0/lib/libkse/arch/arm/Makefile.inc 209118 2010-06-13 01:27:29Z imp $

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S
