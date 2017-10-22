# $FreeBSD: stable/9/lib/clang/libclanganalysis/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	clanganalysis

SRCDIR=	tools/clang/lib/Analysis
SRCS=	AnalysisDeclContext.cpp \
	BodyFarm.cpp \
	CFG.cpp \
	CFGReachabilityAnalysis.cpp \
	CFGStmtMap.cpp \
	CallGraph.cpp \
	CocoaConventions.cpp \
	Dominators.cpp \
	FormatString.cpp \
	LiveVariables.cpp \
	ObjCNoReturn.cpp \
	PostOrderCFGView.cpp \
	PrintfFormatString.cpp \
	ProgramPoint.cpp \
	PseudoConstantAnalysis.cpp \
	ReachableCode.cpp \
	ScanfFormatString.cpp \
	ThreadSafety.cpp \
	UninitializedValues.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentNodes \
	DeclNodes \
	DiagnosticAnalysisKinds \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
