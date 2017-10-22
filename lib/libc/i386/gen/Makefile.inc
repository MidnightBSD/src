#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/lib/libc/i386/gen/Makefile.inc 230429 2012-01-21 18:00:28Z kib $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c fabs.S \
	flt_rounds.c getcontextx.c infinity.c ldexp.c makecontext.c \
	rfork_thread.S setjmp.S signalcontext.c sigsetjmp.S
