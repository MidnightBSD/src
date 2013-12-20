# $FreeBSD: release/9.2.0/usr.bin/clang/llvm-prof/Makefile 252723 2013-07-04 20:17:04Z dim $

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
