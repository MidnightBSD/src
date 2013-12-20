# $FreeBSD: release/9.2.0/usr.bin/clang/opt/Makefile 252723 2013-07-04 20:17:04Z dim $

.include <bsd.own.mk>

PROG_CXX=opt

SRCDIR=	tools/opt
SRCS=	AnalysisWrappers.cpp \
	GraphPrinters.cpp \
	PrintSCC.cpp \
	opt.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmarmdisassembler \
	llvmarmcodegen \
	llvmarmasmparser \
	llvmarmdesc \
	llvmarminfo \
	llvmarminstprinter \
	llvmmipsdisassembler \
	llvmmipscodegen \
	llvmmipsasmparser \
	llvmmipsdesc \
	llvmmipsinfo \
	llvmmipsinstprinter \
	llvmpowerpccodegen \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcinstprinter \
	llvmx86disassembler \
	llvmx86asmparser \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvmcodegen \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmipo \
	llvmvectorize \
	llvmobjcarcopts \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmirreader \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
