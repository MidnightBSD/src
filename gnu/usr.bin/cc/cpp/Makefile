# $MidnightBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.3 2009/05/22 00:51:28 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.27 2007/05/19 04:25:58 kan Exp $

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
