# $FreeBSD: stable/9/lib/clang/libclangrewritefrontend/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	clangrewritefrontend

SRCDIR=	tools/clang/lib/Rewrite/Frontend
SRCS=	FixItRewriter.cpp \
	FrontendActions.cpp \
	HTMLPrint.cpp \
	InclusionRewriter.cpp \
	RewriteMacros.cpp \
	RewriteModernObjC.cpp \
	RewriteObjC.cpp \
	RewriteTest.cpp

TGHDRS=	AttrList \
	AttrParsedAttrList \
	Attrs \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	StmtNodes

.include "../clang.lib.mk"
