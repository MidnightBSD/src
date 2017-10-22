#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/9/lib/libc/arm/gen/Makefile.inc 237491 2012-06-23 18:39:35Z marius $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c alloca.S fabs.c \
	getcontextx.c infinity.c ldexp.c makecontext.c modf.c \
	__aeabi_read_tp.S setjmp.S signalcontext.c sigsetjmp.S divsi3.S
