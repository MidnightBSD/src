# $FreeBSD: stable/9/lib/clang/libllvmarmdesc/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmarmdesc

SRCDIR=	lib/Target/ARM/MCTargetDesc
SRCS=	ARMAsmBackend.cpp \
	ARMELFObjectWriter.cpp \
	ARMMachObjectWriter.cpp \
	ARMMCAsmInfo.cpp \
	ARMMCCodeEmitter.cpp \
	ARMMCExpr.cpp \
	ARMMCTargetDesc.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	ARMGenInstrInfo \
	ARMGenMCCodeEmitter \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
