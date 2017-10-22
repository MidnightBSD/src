# $FreeBSD: stable/9/lib/clang/libclangserialization/Makefile 246637 2013-02-10 21:24:47Z dim $

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
	Module.cpp \
	ModuleManager.cpp

TGHDRS=	AttrList \
	AttrPCHRead \
	AttrPCHWrite \
	AttrParsedAttrList \
	Attrs \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	DiagnosticSemaKinds \
	DiagnosticSerializationKinds \
	StmtNodes

.include "../clang.lib.mk"
