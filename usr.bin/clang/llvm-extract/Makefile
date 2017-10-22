# $FreeBSD: stable/9/usr.bin/clang/llvm-extract/Makefile 245431 2013-01-14 19:51:26Z dim $

PROG_CXX=llvm-extract

SRCDIR=	tools/llvm-extract
SRCS=	llvm-extract.cpp

LIBDEPS=llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
