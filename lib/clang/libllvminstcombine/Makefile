# $FreeBSD: stable/9/lib/clang/libllvminstcombine/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvminstcombine

SRCDIR=	lib/Transforms/InstCombine
SRCS=	InstCombineAddSub.cpp \
	InstCombineAndOrXor.cpp \
	InstCombineCalls.cpp \
	InstCombineCasts.cpp \
	InstCombineCompares.cpp \
	InstCombineLoadStoreAlloca.cpp \
	InstCombineMulDivRem.cpp \
	InstCombinePHI.cpp \
	InstCombineSelect.cpp \
	InstCombineShifts.cpp \
	InstCombineSimplifyDemanded.cpp \
	InstCombineVectorOps.cpp \
	InstructionCombining.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
