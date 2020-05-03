# $FreeBSD: stable/11/lib/clang/clang.pre.mk 310618 2016-12-26 20:36:37Z dim $

.include "llvm.pre.mk"

CLANG_SRCS=	${LLVM_SRCS}/tools/clang

CLANG_TBLGEN?=	clang-tblgen
