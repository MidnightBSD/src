# $FreeBSD: stable/9/lib/clang/libllvmmipsdisassembler/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmmipsdisassembler

SRCDIR=	lib/Target/Mips/Disassembler
INCDIR=	lib/Target/Mips
SRCS=	MipsDisassembler.cpp

TGHDRS=	MipsGenDisassemblerTables \
	MipsGenEDInfo \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
