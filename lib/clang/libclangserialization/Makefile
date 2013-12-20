# $FreeBSD: release/9.2.0/lib/clang/libclangserialization/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	clangserialization

SRCDIR=	tools/clang/lib/Serialization
SRCS=	ASTCommon.cpp \
	ASTReader.cpp \
	ASTReaderDecl.cpp \
	ASTReaderStmt.cpp \
	ASTWriter.cpp \
	ASTWriterDecl.cpp \
	ASTWriterStmt.cpp \
	GeneratePCH.cpp \
	GlobalModuleIndex.cpp \
	Module.cpp \
	ModuleManager.cpp

TGHDRS=	AttrList \
	AttrPCHRead \
	AttrPCHWrite \
	AttrParsedAttrList \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	DiagnosticSemaKinds \
	DiagnosticSerializationKinds \
	StmtNodes

.include "../clang.lib.mk"
