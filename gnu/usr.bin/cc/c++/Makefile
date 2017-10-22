# $FreeBSD: stable/9/gnu/usr.bin/cc/c++/Makefile 248355 2013-03-15 22:20:45Z brooks $

NO_MAN=
.include <bsd.own.mk>

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}/cp

PROG=	g++
SRCS+=	g++spec.c

DPADD=	${LIBCPP} ${LIBIBERTY}
LDADD=	${LIBCPP} ${LIBIBERTY}

.if ${MK_CLANG_IS_CC} == "no"
LINKS=	${BINDIR}/g++ ${BINDIR}/c++
LINKS+=	${BINDIR}/g++ ${BINDIR}/CC
.endif

.include <bsd.prog.mk>
