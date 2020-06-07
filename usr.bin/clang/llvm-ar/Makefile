# $FreeBSD: stable/11/usr.bin/clang/llvm-ar/Makefile 311934 2017-01-11 20:21:05Z dim $

PROG_CXX=	llvm-ar

SRCDIR=		tools/llvm-ar
SRCS+=		llvm-ar.cpp

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-ar ${BINDIR}/llvm-ranlib

.include "../llvm.prog.mk"
