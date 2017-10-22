# $FreeBSD: release/10.0.0/lib/clang/liblldbExpression/Makefile 255722 2013-09-20 01:52:02Z emaste $

.include <bsd.own.mk>

LIB=	lldbExpression

SRCDIR=	tools/lldb/source/Expression
SRCS=	ASTDumper.cpp \
	ASTResultSynthesizer.cpp \
	ASTStructExtractor.cpp \
	ClangASTSource.cpp \
	ClangExpressionDeclMap.cpp \
	ClangExpressionParser.cpp \
	ClangExpressionVariable.cpp \
	ClangFunction.cpp \
	ClangPersistentVariables.cpp \
	ClangUserExpression.cpp \
	ClangUtilityFunction.cpp \
	DWARFExpression.cpp \
	ExpressionSourceCode.cpp \
	IRDynamicChecks.cpp \
	IRExecutionUnit.cpp \
	IRForTarget.cpp \
	IRInterpreter.cpp \
	IRMemoryMap.cpp \
	Materializer.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList \
	AttrList \
	Attrs \
	AttrParsedAttrList \
	DiagnosticFrontendKinds \
	DiagnosticSemaKinds \
	Intrinsics

.include "../lldb.lib.mk"
