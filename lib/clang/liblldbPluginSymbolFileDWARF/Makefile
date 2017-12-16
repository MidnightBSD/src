# $MidnightBSD$
# $FreeBSD: stable/10/lib/clang/liblldbPluginSymbolFileDWARF/Makefile 263363 2014-03-19 13:11:35Z emaste $

.include <bsd.own.mk>

LIB=	lldbPluginSymbolFileDWARF

SRCDIR=	tools/lldb/source/Plugins/SymbolFile/DWARF
SRCS=	DWARFAbbreviationDeclaration.cpp \
	DWARFCompileUnit.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugMacinfo.cpp \
	DWARFDebugMacinfoEntry.cpp \
	DWARFDebugPubnames.cpp \
	DWARFDebugPubnamesSet.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFDIECollection.cpp \
	DWARFFormValue.cpp \
	DWARFLocationDescription.cpp \
	DWARFLocationList.cpp \
	LogChannelDWARF.cpp \
	NameToDIE.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDebugMap.cpp \
	UniqueDWARFASTType.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList \
	AttrParsedAttrList \
	DiagnosticFrontendKinds \
	Intrinsics

.include "../lldb.lib.mk"
