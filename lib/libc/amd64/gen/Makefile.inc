#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/lib/libc/amd64/gen/Makefile.inc 143658 2005-03-15 15:53:39Z das $

SRCS+=	_setjmp.S _set_tp.c rfork_thread.S setjmp.S sigsetjmp.S \
	fabs.S modf.S \
	infinity.c ldexp.c makecontext.c signalcontext.c \
	flt_rounds.c fpgetmask.c fpsetmask.c fpgetprec.c fpsetprec.c \
	fpgetround.c fpsetround.c fpgetsticky.c
