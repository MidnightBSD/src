# $FreeBSD: release/9.2.0/lib/clang/libllvmarmdisassembler/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	llvmarmdisassembler

SRCDIR=	lib/Target/ARM/Disassembler
INCDIR=	lib/Target/ARM
SRCS=	ARMDisassembler.cpp

TGHDRS=	ARMGenDisassemblerTables \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
