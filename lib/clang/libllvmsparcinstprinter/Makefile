# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmsparcinstprinter/Makefile 262263 2014-02-20 22:22:39Z dim $

.include <bsd.own.mk>

LIB=	llvmsparcinstprinter

SRCDIR=	lib/Target/Sparc/InstPrinter
INCDIR=	lib/Target/Sparc
SRCS=	SparcInstPrinter.cpp

TGHDRS=	SparcGenAsmWriter \
	SparcGenInstrInfo \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
