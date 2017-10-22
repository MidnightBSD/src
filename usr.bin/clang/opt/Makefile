# $FreeBSD: stable/9/usr.bin/clang/opt/Makefile 245431 2013-01-14 19:51:26Z dim $

PROG_CXX=opt

SRCDIR=	tools/opt
SRCS=	AnalysisWrappers.cpp \
	GraphPrinters.cpp \
	PrintSCC.cpp \
	opt.cpp

TGHDRS=	Intrinsics
LIBDEPS=llvmarmdisassembler \
	llvmarmasmparser \
	llvmarmcodegen \
	llvmarmdesc \
	llvmarminfo \
	llvmarminstprinter \
	llvmmipsasmparser \
	llvmmipscodegen \
	llvmmipsdesc \
	llvmmipsinstprinter \
	llvmmipsdisassembler \
	llvmmipsinfo \
	llvmpowerpccodegen \
	llvmpowerpcdesc \
	llvmpowerpcinfo \
	llvmpowerpcinstprinter \
	llvmx86asmparser \
	llvmx86codegen \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvmcodegen \
	llvmx86disassembler \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmipo \
	llvmvectorize \
	llvmscalaropts \
	llvminstcombine \
	llvminstrumentation \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmasmparser \
	llvmbitwriter \
	llvmbitreader \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
