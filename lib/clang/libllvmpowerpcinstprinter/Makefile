# $FreeBSD: stable/9/lib/clang/libllvmpowerpcinstprinter/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmpowerpcinstprinter

SRCDIR=	lib/Target/PowerPC/InstPrinter
INCDIR=	lib/Target/PowerPC
SRCS=	PPCInstPrinter.cpp

TGHDRS=	PPCGenAsmWriter \
	PPCGenInstrInfo \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
