
SRCS += _ctx_start.S fabs.S flt_rounds.c fpgetmask.c fpgetround.c \
	fpgetsticky.c fpsetmask.c fpsetround.c _get_tp.c \
	infinity.c ldexp.c makecontext.c _setjmp.S \
	setjmp.S sigsetjmp.S signalcontext.c syncicache.c \
	_set_tp.c \
	trivial-getcontextx.c

CFLAGS._get_tp.c+= ${RTLD_HDRS}

