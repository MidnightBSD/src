# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmsparcdisassembler/Makefile 262263 2014-02-20 22:22:39Z dim $

.include <bsd.own.mk>

LIB=	llvmsparcdisassembler

SRCDIR=	lib/Target/Sparc/Disassembler
INCDIR=	lib/Target/Sparc
SRCS=	SparcDisassembler.cpp

TGHDRS=	SparcGenDisassemblerTables \
	SparcGenInstrInfo \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
