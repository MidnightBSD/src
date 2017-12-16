# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libllvmpowerpccodegen/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

LIB=	llvmpowerpccodegen

SRCDIR=	lib/Target/PowerPC
SRCS=	PPCAsmPrinter.cpp \
	PPCBranchSelector.cpp \
	PPCCTRLoops.cpp \
	PPCCodeEmitter.cpp \
	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCHazardRecognizers.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCInstrInfo.cpp \
	PPCJITInfo.cpp \
	PPCMCInstLower.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCRegisterInfo.cpp \
	PPCSelectionDAGInfo.cpp \
	PPCSubtarget.cpp \
	PPCTargetMachine.cpp \
	PPCTargetObjectFile.cpp \
	PPCTargetTransformInfo.cpp

TGHDRS=	Intrinsics \
	PPCGenCallingConv \
	PPCGenCodeEmitter \
	PPCGenDAGISel \
	PPCGenFastISel \
	PPCGenInstrInfo \
	PPCGenMCCodeEmitter \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
