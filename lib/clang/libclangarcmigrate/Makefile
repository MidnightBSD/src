# $FreeBSD: release/9.2.0/lib/clang/libclangarcmigrate/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

LIB=	clangarcmigrate

SRCDIR=	tools/clang/lib/ARCMigrate
SRCS=	ARCMT.cpp \
	ARCMTActions.cpp \
	FileRemapper.cpp \
	ObjCMT.cpp \
	PlistReporter.cpp \
	TransAPIUses.cpp \
	TransARCAssign.cpp \
	TransAutoreleasePool.cpp \
	TransBlockObjCVariable.cpp \
	TransEmptyStatementsAndDealloc.cpp \
	TransGCAttrs.cpp \
	TransGCCalls.cpp \
	TransProperties.cpp \
	TransProtectedScope.cpp \
	TransRetainReleaseDealloc.cpp \
	TransUnbridgedCasts.cpp \
	TransUnusedInitDelegate.cpp \
	TransZeroOutPropsInDealloc.cpp \
	TransformActions.cpp \
	Transforms.cpp

TGHDRS=	AttrList \
	AttrParsedAttrList \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticGroups \
	DiagnosticSemaKinds \
	StmtNodes

.include "../clang.lib.mk"
