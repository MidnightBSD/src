# $FreeBSD: stable/9/lib/clang/libllvmmipsinstprinter/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmmipsinstprinter

SRCDIR=	lib/Target/Mips/InstPrinter
INCDIR=	lib/Target/Mips
SRCS=	MipsInstPrinter.cpp

TGHDRS=	MipsGenAsmWriter \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
