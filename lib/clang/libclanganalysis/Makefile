# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libclanganalysis/Makefile 263508 2014-03-21 17:53:59Z dim $

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
	Consumed.cpp \
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
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticAnalysisKinds \
	DiagnosticCommonKinds \
	StmtNodes

.include "../clang.lib.mk"
