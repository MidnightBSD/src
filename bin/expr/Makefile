# $FreeBSD: stable/11/bin/expr/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <src.opts.mk>

PACKAGE=runtime
PROG=	expr
SRCS=	expr.y
YFLAGS=

# expr relies on signed integer wrapping
CFLAGS+= -fwrapv

NO_WMISSING_VARIABLE_DECLARATIONS=

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
