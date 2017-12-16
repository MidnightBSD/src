# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvminstrumentation/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

LIB=	llvminstrumentation

SRCDIR=	lib/Transforms/Instrumentation
SRCS=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	DataFlowSanitizer.cpp \
	DebugIR.cpp \
	GCOVProfiling.cpp \
	MemorySanitizer.cpp \
	Instrumentation.cpp \
	ThreadSanitizer.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
