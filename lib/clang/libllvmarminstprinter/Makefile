# $FreeBSD: stable/9/lib/clang/libllvmarminstprinter/Makefile 246637 2013-02-10 21:24:47Z dim $

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
