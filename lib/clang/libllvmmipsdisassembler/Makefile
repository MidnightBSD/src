# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmmipsdisassembler/Makefile 249423 2013-04-12 17:57:40Z dim $

.include <bsd.own.mk>

LIB=	llvmmipsdisassembler

SRCDIR=	lib/Target/Mips/Disassembler
INCDIR=	lib/Target/Mips
SRCS=	MipsDisassembler.cpp

TGHDRS=	MipsGenDisassemblerTables \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
