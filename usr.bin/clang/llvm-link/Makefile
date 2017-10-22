# $FreeBSD: stable/9/usr.bin/clang/llvm-link/Makefile 245431 2013-01-14 19:51:26Z dim $

PROG_CXX=llvm-link

SRCDIR=	tools/llvm-link
SRCS=	llvm-link.cpp

LIBDEPS=llvmasmparser \
	llvmbitwriter \
	llvmlinker \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmarchive \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
