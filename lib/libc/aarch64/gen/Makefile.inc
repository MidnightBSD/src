# $FreeBSD: stable/11/lib/libc/aarch64/gen/Makefile.inc 297619 2016-04-06 16:09:10Z andrew $

CFLAGS+=	-DNO_COMPAT7

SRCS+=	_ctx_start.S \
	fabs.S \
	flt_rounds.c \
	fpgetmask.c \
	fpsetmask.c \
	infinity.c \
	ldexp.c \
	makecontext.c \
	_setjmp.S \
	_set_tp.c \
	setjmp.S \
	sigsetjmp.S \
	trivial-getcontextx.c
