# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmmipsdesc/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

LIB=	llvmmipsdesc

SRCDIR=	lib/Target/Mips/MCTargetDesc
SRCS=	MipsAsmBackend.cpp \
	MipsELFObjectWriter.cpp \
	MipsMCAsmInfo.cpp \
	MipsMCCodeEmitter.cpp \
	MipsMCTargetDesc.cpp \
	MipsReginfo.cpp \
	MipsTargetStreamer.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	MipsGenInstrInfo \
	MipsGenMCCodeEmitter \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
