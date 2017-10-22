# $FreeBSD: stable/9/lib/clang/libllvmarmasmparser/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmarmasmparser

SRCDIR=	lib/Target/ARM/AsmParser
INCDIR=	lib/Target/ARM
SRCS=	ARMAsmLexer.cpp \
	ARMAsmParser.cpp

TGHDRS=	ARMGenAsmMatcher \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
