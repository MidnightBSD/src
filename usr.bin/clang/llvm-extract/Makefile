# $FreeBSD: release/9.2.0/usr.bin/clang/llvm-extract/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

PROG_CXX=llvm-extract

SRCDIR=	tools/llvm-extract
SRCS=	llvm-extract.cpp

LIBDEPS=llvmirreader \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmipo \
	llvmobjcarcopts \
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
