# $FreeBSD: release/10.0.0/lib/clang/liblldbPluginProcessGDBRemote/Makefile 255722 2013-09-20 01:52:02Z emaste $

.include <bsd.own.mk>

LIB=	lldbPluginProcessGDBRemote

SRCDIR=	tools/lldb/source/Plugins/Process/gdb-remote
SRCS=	GDBRemoteCommunication.cpp \
	GDBRemoteCommunicationClient.cpp \
	GDBRemoteCommunicationServer.cpp \
	GDBRemoteRegisterContext.cpp \
	ProcessGDBRemote.cpp \
	ProcessGDBRemoteLog.cpp \
	ThreadGDBRemote.cpp

TGHDRS=	DiagnosticCommonKinds \
	DeclNodes \
	StmtNodes \
	CommentCommandList

.include "../lldb.lib.mk"
