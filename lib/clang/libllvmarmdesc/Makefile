# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmarmdesc/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

LIB=	llvmarmdesc

SRCDIR=	lib/Target/ARM/MCTargetDesc
SRCS=	ARMAsmBackend.cpp \
	ARMELFObjectWriter.cpp \
	ARMELFStreamer.cpp \
	ARMMCAsmInfo.cpp \
	ARMMCCodeEmitter.cpp \
	ARMMCExpr.cpp \
	ARMMCTargetDesc.cpp \
	ARMMachORelocationInfo.cpp \
	ARMMachObjectWriter.cpp \
	ARMUnwindOpAsm.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	ARMGenInstrInfo \
	ARMGenMCCodeEmitter \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
