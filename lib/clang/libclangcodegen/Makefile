# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/libclangcodegen/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

LIB=	clangcodegen

SRCDIR=	tools/clang/lib/CodeGen
SRCS=	BackendUtil.cpp \
	CGAtomic.cpp \
	CGBlocks.cpp \
	CGBuiltin.cpp \
	CGCUDANV.cpp \
	CGCUDARuntime.cpp \
	CGCXX.cpp \
	CGCXXABI.cpp \
	CGCall.cpp \
	CGClass.cpp \
	CGCleanup.cpp \
	CGDebugInfo.cpp \
	CGDecl.cpp \
	CGDeclCXX.cpp \
	CGException.cpp \
	CGExpr.cpp \
	CGExprAgg.cpp \
	CGExprCXX.cpp \
	CGExprComplex.cpp \
	CGExprConstant.cpp \
	CGExprScalar.cpp \
	CGObjC.cpp \
	CGObjCGNU.cpp \
	CGObjCMac.cpp \
	CGObjCRuntime.cpp \
	CGOpenCLRuntime.cpp \
	CGRTTI.cpp \
	CGRecordLayoutBuilder.cpp \
	CGStmt.cpp \
	CGVTT.cpp \
	CGVTables.cpp \
	CodeGenABITypes.cpp \
	CodeGenAction.cpp \
	CodeGenFunction.cpp \
	CodeGenModule.cpp \
	CodeGenTBAA.cpp \
	CodeGenTypes.cpp \
	ItaniumCXXABI.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftVBTables.cpp \
	ModuleBuilder.cpp \
	TargetInfo.cpp

TGHDRS=	AttrList \
	Attrs \
	CommentCommandList \
	CommentNodes \
	DeclNodes \
	DiagnosticCommonKinds \
	DiagnosticFrontendKinds \
	DiagnosticSemaKinds \
	Intrinsics \
	StmtNodes \
	arm_neon

.include "../clang.lib.mk"
