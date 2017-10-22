# $FreeBSD: release/10.0.0/lib/libkse/arch/powerpc/Makefile.inc 209118 2010-06-13 01:27:29Z imp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	enter_uts.S context.S pthread_md.c
