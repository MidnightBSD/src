#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/lib/libc/arm/gen/Makefile.inc 135684 2004-09-23 23:12:57Z cognet $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c alloca.S fabs.c \
	infinity.c ldexp.c makecontext.c modf.c \
	setjmp.S signalcontext.c sigsetjmp.S divsi3.S
