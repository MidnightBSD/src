# $FreeBSD: stable/9/lib/clang/libllvmmipsasmparser/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmmipsasmparser

SRCDIR=	lib/Target/Mips/AsmParser
INCDIR=	lib/Target/Mips
SRCS=	MipsAsmParser.cpp

TGHDRS=	MipsGenAsmMatcher \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
