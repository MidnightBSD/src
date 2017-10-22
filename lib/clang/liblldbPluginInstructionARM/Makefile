# $FreeBSD: release/10.0.0/lib/clang/liblldbPluginInstructionARM/Makefile 255722 2013-09-20 01:52:02Z emaste $

.include <bsd.own.mk>

LIB=	lldbPluginInstructionARM

SRCDIR=	tools/lldb/source/Plugins/Instruction/ARM
SRCS=	EmulateInstructionARM.cpp \
	EmulationStateARM.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
