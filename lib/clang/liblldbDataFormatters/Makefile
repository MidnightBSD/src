# $FreeBSD: release/10.0.0/lib/clang/liblldbDataFormatters/Makefile 255722 2013-09-20 01:52:02Z emaste $

.include <bsd.own.mk>

LIB=	lldbDataFormatters

SRCDIR=	tools/lldb/source/DataFormatters
SRCS=	CF.cpp \
	Cocoa.cpp \
	CXXFormatterFunctions.cpp \
	DataVisualization.cpp \
	FormatCache.cpp \
	FormatClasses.cpp \
	FormatManager.cpp \
	LibCxx.cpp \
	LibCxxList.cpp \
	LibCxxMap.cpp \
	LibStdcpp.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSSet.cpp \
	TypeCategory.cpp \
	TypeCategoryMap.cpp \
	TypeFormat.cpp \
	TypeSummary.cpp \
	TypeSynthetic.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
