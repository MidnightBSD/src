# $FreeBSD: release/10.0.0/lib/clang/liblldbPluginDynamicLoaderPosixDYLD/Makefile 255722 2013-09-20 01:52:02Z emaste $

.include <bsd.own.mk>

LIB=	lldbPluginDynamicLoaderPosixDYLD

SRCDIR=	tools/lldb/source/Plugins/DynamicLoader/POSIX-DYLD
SRCS=	AuxVector.cpp \
	DYLDRendezvous.cpp \
	DynamicLoaderPOSIXDYLD.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
