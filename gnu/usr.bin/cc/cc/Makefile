# $FreeBSD: release/7.0.0/gnu/usr.bin/cc/cc/Makefile 169718 2007-05-19 04:25:59Z kan $

.include "../Makefile.inc"
.include "../Makefile.fe"

PROG=	cc
MAN=	gcc.1
SRCS+=  gccspec.c

NO_SHARED?=yes

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 CC.1

.include <bsd.prog.mk>
