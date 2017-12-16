# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/clang/lli/Makefile 263508 2014-03-21 17:53:59Z dim $

.include <bsd.own.mk>

PROG_CXX=lli

SRCDIR=	tools/lli
SRCS=	lli.cpp \
	RemoteMemoryManager.cpp \
	RemoteTarget.cpp \
	RemoteTargetExternal.cpp

LIBDEPS=llvmx86disassembler \
	llvmx86asmparser \
	llvmirreader \
	llvmasmparser \
	llvmbitreader \
	llvmx86codegen \
	llvmx86desc \
	llvmx86info \
	llvmx86instprinter \
	llvmx86utils \
	llvmselectiondag \
	llvmasmprinter \
	llvmmcparser \
	llvminterpreter \
	llvminstrumentation \
	llvmmcjit \
	llvmjit \
	llvmcodegen \
	llvmobjcarcopts \
	llvmscalaropts \
	llvminstcombine \
	llvmtransformutils \
	llvmipa \
	llvmanalysis \
	llvmruntimedyld \
	llvmexecutionengine \
	llvmtarget \
	llvmmc \
	llvmobject \
	llvmcore \
	llvmsupport

.include "../clang.prog.mk"
