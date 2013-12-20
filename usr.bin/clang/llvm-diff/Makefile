# $FreeBSD: release/9.2.0/usr.bin/clang/llvm-diff/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

PROG_CXX=llvm-diff

SRCDIR=	tools/llvm-diff
SRCS=	llvm-diff.cpp \
	DiffConsumer.cpp \
	DiffLog.cpp \
	DifferenceEngine.cpp

LIBDEPS=llvmirreader \
	llvmbitreader \
	llvmasmparser \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
