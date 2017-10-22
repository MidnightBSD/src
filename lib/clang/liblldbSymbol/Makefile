# $FreeBSD: release/10.0.0/lib/clang/liblldbSymbol/Makefile 255722 2013-09-20 01:52:02Z emaste $

.include <bsd.own.mk>

LIB=	lldbSymbol

SRCDIR=	tools/lldb/source/Symbol
SRCS=	Block.cpp \
	ClangASTContext.cpp \
	ClangASTImporter.cpp \
	ClangASTType.cpp \
	ClangExternalASTSourceCallbacks.cpp \
	ClangExternalASTSourceCommon.cpp \
	ClangNamespaceDecl.cpp \
	CompileUnit.cpp \
	Declaration.cpp \
	DWARFCallFrameInfo.cpp \
	Function.cpp \
	FuncUnwinders.cpp \
	LineEntry.cpp \
	LineTable.cpp \
	ObjectFile.cpp \
	Symbol.cpp \
	SymbolContext.cpp \
	SymbolFile.cpp \
	SymbolVendor.cpp \
	Symtab.cpp \
	Type.cpp \
	TypeList.cpp \
	UnwindPlan.cpp \
	UnwindTable.cpp \
	Variable.cpp \
	VariableList.cpp \
	VerifyDecl.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList \
	AttrList \
	Attrs

.include "../lldb.lib.mk"
