# $FreeBSD: stable/11/lib/libc/riscv/gen/Makefile.inc 294227 2016-01-17 15:21:23Z br $

SRCS+=	_ctx_start.S \
	fabs.S \
	flt_rounds.c \
	infinity.c \
	ldexp.c \
	makecontext.c \
	_setjmp.S \
	_set_tp.c \
	setjmp.S \
	sigsetjmp.S \
	trivial-getcontextx.c
