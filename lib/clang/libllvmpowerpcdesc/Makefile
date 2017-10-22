# $FreeBSD: stable/9/lib/clang/libllvmpowerpcdesc/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmpowerpcdesc

SRCDIR=	lib/Target/PowerPC/MCTargetDesc
SRCS=	PPCAsmBackend.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCMCTargetDesc.cpp \
	PPCPredicates.cpp \
	PPCELFObjectWriter.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	PPCGenInstrInfo \
	PPCGenMCCodeEmitter \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
