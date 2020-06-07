# $FreeBSD: stable/11/usr.bin/clang/llvm-cov/Makefile 346296 2019-04-16 20:05:24Z dim $

.include <src.opts.mk>

PROG_CXX=	llvm-cov
.if ${MK_GCOV} == "no"
LINKS=		${BINDIR}/llvm-cov	${BINDIR}/gcov
MLINKS=		llvm-cov.1		gcov.1
.endif

SRCDIR=		tools/llvm-cov
SRCS+=		CodeCoverage.cpp
SRCS+=		CoverageExporterJson.cpp
SRCS+=		CoverageExporterLcov.cpp
SRCS+=		CoverageFilters.cpp
SRCS+=		CoverageReport.cpp
SRCS+=		CoverageSummaryInfo.cpp
SRCS+=		SourceCoverageView.cpp
SRCS+=		SourceCoverageViewHTML.cpp
SRCS+=		SourceCoverageViewText.cpp
SRCS+=		TestingSupport.cpp
SRCS+=		gcov.cpp
SRCS+=		llvm-cov.cpp

LIBADD+=	z

.include "../llvm.prog.mk"
