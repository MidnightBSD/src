# $FreeBSD: stable/9/usr.bin/clang/clang-tblgen/Makefile 245431 2013-01-14 19:51:26Z dim $

PROG_CXX=clang-tblgen
NO_MAN=

SRCDIR=	tools/clang/utils/TableGen
SRCS=	ClangASTNodesEmitter.cpp \
	ClangAttrEmitter.cpp \
	ClangCommentCommandInfoEmitter.cpp \
	ClangCommentHTMLTagsEmitter.cpp \
	ClangDiagnosticsEmitter.cpp \
	ClangSACheckersEmitter.cpp \
	NeonEmitter.cpp \
	OptParserEmitter.cpp \
	TableGen.cpp
LLVM_REQUIRES_EH=

LIBDEPS=llvmtablegen \
	llvmsupport

.include "../clang.prog.mk"
