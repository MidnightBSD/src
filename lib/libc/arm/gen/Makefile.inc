#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/lib/libc/arm/gen/Makefile.inc 251514 2013-06-07 22:01:06Z andrew $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c alloca.S fabs.c \
	getcontextx.c infinity.c ldexp.c makecontext.c \
	__aeabi_read_tp.S setjmp.S signalcontext.c sigsetjmp.S flt_rounds.c \
	arm_initfini.c

.if ${MK_ARM_EABI} == "no"
SRCS+=	divsi3.S
.endif
