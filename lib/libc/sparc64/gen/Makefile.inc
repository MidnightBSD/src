# $FreeBSD: release/10.0.0/lib/libc/sparc64/gen/Makefile.inc 230429 2012-01-21 18:00:28Z kib $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c \
	getcontextx.c infinity.c ldexp.c makecontext.c \
	signalcontext.c setjmp.S sigsetjmp.S _set_tp.c
