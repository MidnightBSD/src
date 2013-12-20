# $FreeBSD: release/9.2.0/lib/clang/libclangstaticanalyzerfrontend/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	clangstaticanalyzerfrontend

SRCDIR=	tools/clang/lib/StaticAnalyzer/Frontend
SRCS=	AnalysisConsumer.cpp \
	CheckerRegistration.cpp \
	FrontendActions.cpp

TGHDRS=	AttrList \
	Attrs \
	Checkers \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	StmtNodes

.include "../clang.lib.mk"
