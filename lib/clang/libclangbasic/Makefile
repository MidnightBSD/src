# $MidnightBSD$
# $FreeBSD: release/9.2.0/lib/clang/libclangbasic/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	clangbasic

SRCDIR=	tools/clang/lib/Basic
SRCS=	Builtins.cpp \
	CharInfo.cpp \
	Diagnostic.cpp \
	DiagnosticIDs.cpp \
	FileManager.cpp \
	FileSystemStatCache.cpp \
	IdentifierTable.cpp \
	LangOptions.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	TargetInfo.cpp \
	Targets.cpp \
	TokenKinds.cpp \
	Version.cpp \
	VersionTuple.cpp

TGHDRS=	DiagnosticAnalysisKinds \
	DiagnosticASTKinds \
	DiagnosticCommentKinds \
	DiagnosticCommonKinds \
	DiagnosticDriverKinds \
	DiagnosticFrontendKinds \
	DiagnosticGroups \
	DiagnosticIndexName \
	DiagnosticLexKinds \
	DiagnosticParseKinds \
	DiagnosticSemaKinds \
	DiagnosticSerializationKinds \
	arm_neon

.include "../clang.lib.mk"
