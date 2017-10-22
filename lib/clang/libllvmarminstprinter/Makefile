# $FreeBSD: release/10.0.0/lib/clang/libllvmarminstprinter/Makefile 246259 2013-02-02 22:28:29Z dim $

.include <bsd.own.mk>

LIB=	llvmarminstprinter

SRCDIR=	lib/Target/ARM/InstPrinter
INCDIR=	lib/Target/ARM
SRCS=	ARMInstPrinter.cpp

TGHDRS=	ARMGenAsmWriter \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
