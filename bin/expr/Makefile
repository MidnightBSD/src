# $MidnightBSD$
# $FreeBSD: stable/10/bin/expr/Makefile 281148 2015-04-06 14:45:40Z dim $

.include <bsd.own.mk>

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
