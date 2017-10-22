# $FreeBSD: stable/9/lib/clang/libllvmipa/Makefile 231919 2012-02-19 23:35:56Z dim $

.include <bsd.own.mk>

LIB=	llvmipa

SRCDIR=	lib/Analysis/IPA
SRCS=	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	FindUsedTypes.cpp \
	GlobalsModRef.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	IPA.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
