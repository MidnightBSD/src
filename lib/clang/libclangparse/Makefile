# $FreeBSD: release/9.2.0/lib/clang/libclangparse/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	clangparse

SRCDIR=	tools/clang/lib/Parse
SRCS=	ParseAST.cpp \
	ParseCXXInlineMethods.cpp \
	ParseDecl.cpp \
	ParseDeclCXX.cpp \
	ParseExpr.cpp \
	ParseExprCXX.cpp \
	ParseInit.cpp \
	ParseObjc.cpp \
	ParseOpenMP.cpp \
	ParsePragma.cpp \
	ParseStmt.cpp \
	ParseTemplate.cpp \
	ParseTentative.cpp \
	Parser.cpp

TGHDRS=	AttrLateParsed \
	AttrList \
	AttrParsedAttrList \
	AttrExprArgs \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticParseKinds \
	DiagnosticSemaKinds \
	StmtNodes

.include "../clang.lib.mk"
