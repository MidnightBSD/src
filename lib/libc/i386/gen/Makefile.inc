#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/lib/libc/i386/gen/Makefile.inc 162039 2006-09-05 16:21:26Z ru $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c fabs.S \
	flt_rounds.c infinity.c ldexp.c makecontext.c modf.S \
	rfork_thread.S setjmp.S signalcontext.c sigsetjmp.S
