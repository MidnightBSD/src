# $FreeBSD: release/10.0.0/usr.bin/clang/llvm-prof/Makefile 246705 2013-02-12 05:56:00Z andrew $

.include <bsd.own.mk>

PROG_CXX=llvm-prof

SRCDIR=	tools/llvm-prof
SRCS=	llvm-prof.cpp

LIBDEPS=llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
