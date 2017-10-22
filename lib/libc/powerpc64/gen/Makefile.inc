# $FreeBSD: stable/9/lib/libc/powerpc64/gen/Makefile.inc 231981 2012-02-21 21:18:59Z kib $

SRCS += _ctx_start.S fabs.S flt_rounds.c fpgetmask.c fpgetround.c \
	fpgetsticky.c fpsetmask.c fpsetround.c getcontextx.c \
	infinity.c ldexp.c makecontext.c modf.c _setjmp.S \
	setjmp.S sigsetjmp.S signalcontext.c syncicache.c \
	_set_tp.c


