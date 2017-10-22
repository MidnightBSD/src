# $FreeBSD: stable/9/gnu/usr.bin/cc/cpp/Makefile 233099 2012-03-17 22:29:05Z dim $

.include <bsd.own.mk>

.include "../Makefile.inc"
.include "../Makefile.fe"

PROG=	gcpp
SRCS+=	cppspec.c

.if ${MK_CLANG_IS_CC} == "no"
LINKS=	${BINDIR}/gcpp ${BINDIR}/cpp
MLINKS=	gcpp.1 cpp.1
.endif

.include <bsd.prog.mk>
