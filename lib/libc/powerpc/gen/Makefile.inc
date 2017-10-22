# $FreeBSD: release/10.0.0/lib/libc/powerpc/gen/Makefile.inc 235929 2012-05-24 20:12:46Z marcel $

SRCS += _ctx_start.S eabi.S fabs.S flt_rounds.c fpgetmask.c fpgetround.c \
	fpgetsticky.c fpsetmask.c fpsetround.c getcontextx.c \
	infinity.c ldexp.c makecontext.c _setjmp.S \
	setjmp.S sigsetjmp.S signalcontext.c syncicache.c \
	_set_tp.c


