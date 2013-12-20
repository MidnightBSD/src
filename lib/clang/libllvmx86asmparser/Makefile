# $FreeBSD: release/9.2.0/lib/clang/libllvmx86asmparser/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	llvmx86asmparser

SRCDIR=	lib/Target/X86/AsmParser
INCDIR=	lib/Target/X86
SRCS=	X86AsmParser.cpp

TGHDRS=	X86GenAsmMatcher \
	X86GenInstrInfo \
	X86GenRegisterInfo \
	X86GenSubtargetInfo

.include "../clang.lib.mk"
