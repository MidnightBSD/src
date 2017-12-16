# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/clang/llvm-nm/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

PROG_CXX=llvm-nm

SRCDIR=	tools/llvm-nm
SRCS=	llvm-nm.cpp

LIBDEPS=llvmobject \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
