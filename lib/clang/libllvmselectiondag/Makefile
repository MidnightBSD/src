# $FreeBSD: stable/9/lib/clang/libllvmselectiondag/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmselectiondag

SRCDIR=	lib/CodeGen/SelectionDAG
SRCS=	DAGCombiner.cpp \
	FastISel.cpp \
	FunctionLoweringInfo.cpp \
	InstrEmitter.cpp \
	LegalizeDAG.cpp \
	LegalizeFloatTypes.cpp \
	LegalizeIntegerTypes.cpp \
	LegalizeTypes.cpp \
	LegalizeTypesGeneric.cpp \
	LegalizeVectorOps.cpp \
	LegalizeVectorTypes.cpp \
	ResourcePriorityQueue.cpp \
	ScheduleDAGFast.cpp \
	ScheduleDAGRRList.cpp \
	ScheduleDAGSDNodes.cpp \
	ScheduleDAGVLIW.cpp \
	SelectionDAG.cpp \
	SelectionDAGBuilder.cpp \
	SelectionDAGDumper.cpp \
	SelectionDAGISel.cpp \
	SelectionDAGPrinter.cpp \
	TargetLowering.cpp \
	TargetSelectionDAGInfo.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
