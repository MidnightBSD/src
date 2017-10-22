# $FreeBSD: release/10.0.0/lib/clang/libllvmmipsasmparser/Makefile 246259 2013-02-02 22:28:29Z dim $

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
