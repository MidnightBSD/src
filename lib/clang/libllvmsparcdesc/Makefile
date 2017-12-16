# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmsparcdesc/Makefile 262263 2014-02-20 22:22:39Z dim $

.include <bsd.own.mk>

LIB=	llvmsparcdesc

SRCDIR=	lib/Target/Sparc/MCTargetDesc
SRCS=	SparcAsmBackend.cpp \
	SparcELFObjectWriter.cpp \
	SparcMCAsmInfo.cpp \
	SparcMCCodeEmitter.cpp \
	SparcMCExpr.cpp \
	SparcMCTargetDesc.cpp \
	SparcTargetStreamer.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	SparcGenInstrInfo \
	SparcGenMCCodeEmitter \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
