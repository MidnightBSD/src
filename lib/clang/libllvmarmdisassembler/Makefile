# $FreeBSD: stable/9/lib/clang/libllvmarmdisassembler/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmarmdisassembler

SRCDIR=	lib/Target/ARM/Disassembler
INCDIR=	lib/Target/ARM
SRCS=	ARMDisassembler.cpp

TGHDRS=	ARMGenDisassemblerTables \
	ARMGenEDInfo \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
