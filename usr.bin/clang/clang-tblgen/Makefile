# $FreeBSD: release/9.2.0/usr.bin/clang/clang-tblgen/Makefile 252723 2013-07-04 20:17:04Z dim $

NO_MAN=
.include <bsd.own.mk>

PROG_CXX=clang-tblgen

SRCDIR=	tools/clang/utils/TableGen
SRCS=	ClangASTNodesEmitter.cpp \
	ClangAttrEmitter.cpp \
	ClangCommentCommandInfoEmitter.cpp \
	ClangCommentHTMLNamedCharacterReferenceEmitter.cpp \
	ClangCommentHTMLTagsEmitter.cpp \
	ClangDiagnosticsEmitter.cpp \
	ClangSACheckersEmitter.cpp \
	NeonEmitter.cpp \
	OptParserEmitter.cpp \
	TableGen.cpp

LIBDEPS=llvmtablegen \
	llvmsupport

.include "../clang.prog.mk"
