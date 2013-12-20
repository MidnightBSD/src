# $FreeBSD: release/9.2.0/usr.bin/clang/llvm-mc/Makefile 252723 2013-07-04 20:17:04Z dim $

NO_MAN=
.include <bsd.own.mk>

PROG_CXX=llvm-mc

SRCDIR=	tools/llvm-mc
SRCS=	llvm-mc.cpp \
	Disassembler.cpp

LIBDEPS=llvmmcdisassembler \
	llvmarmdisassembler \
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
	llvmobjcarcopts \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmx86desc \
	llvmx86info \
	llvmtarget \
	llvmx86instprinter \
	llvmmc \
	llvmobject \
	llvmx86utils \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
