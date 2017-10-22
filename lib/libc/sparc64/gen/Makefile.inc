# $FreeBSD: stable/9/lib/libc/sparc64/gen/Makefile.inc 231981 2012-02-21 21:18:59Z kib $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c \
	getcontextx.c  infinity.c ldexp.c makecontext.c modf.S \
	signalcontext.c setjmp.S sigsetjmp.S _set_tp.c
