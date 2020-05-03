# $FreeBSD: stable/11/lib/libc/sparc64/gen/Makefile.inc 277078 2015-01-12 18:13:38Z emaste $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c \
	infinity.c ldexp.c makecontext.c \
	signalcontext.c setjmp.S sigsetjmp.S _set_tp.c \
	trivial-getcontextx.c
