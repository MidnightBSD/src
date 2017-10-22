# $FreeBSD: stable/9/usr.bin/clang/llvm-dis/Makefile 245431 2013-01-14 19:51:26Z dim $

PROG_CXX=llvm-dis

SRCDIR=	tools/llvm-dis
SRCS=	llvm-dis.cpp
LLVM_REQUIRES_EH=

TGHDRS=	Intrinsics
LIBDEPS=llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
