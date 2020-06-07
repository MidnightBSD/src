# $FreeBSD: stable/11/usr.bin/clang/bugpoint/Makefile 310618 2016-12-26 20:36:37Z dim $

PROG_CXX=	bugpoint

SRCDIR=		tools/bugpoint
SRCS+=		BugDriver.cpp
SRCS+=		CrashDebugger.cpp
SRCS+=		ExecutionDriver.cpp
SRCS+=		ExtractFunction.cpp
SRCS+=		FindBugs.cpp
SRCS+=		Miscompilation.cpp
SRCS+=		OptimizerDriver.cpp
SRCS+=		ToolRunner.cpp
SRCS+=		bugpoint.cpp

LIBADD+=	z

.include "../llvm.prog.mk"
