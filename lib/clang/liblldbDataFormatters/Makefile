# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/liblldbDataFormatters/Makefile 263363 2014-03-19 13:11:35Z emaste $

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
	LibCxxUnorderedMap.cpp \
	LibStdcpp.cpp \
	NSArray.cpp \
	NSDictionary.cpp \
	NSSet.cpp \
	TypeCategory.cpp \
	TypeCategoryMap.cpp \
	TypeFormat.cpp \
	TypeSummary.cpp \
	TypeSynthetic.cpp \
	ValueObjectPrinter.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
