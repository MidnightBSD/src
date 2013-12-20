# $FreeBSD: release/9.2.0/usr.bin/clang/llvm-link/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

PROG_CXX=llvm-link

SRCDIR=	tools/llvm-link
SRCS=	llvm-link.cpp

LIBDEPS=llvmirreader \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmlinker \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
