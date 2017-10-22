# $FreeBSD: stable/9/lib/clang/libllvmx86desc/Makefile 246637 2013-02-10 21:24:47Z dim $

.include <bsd.own.mk>

LIB=	llvmx86desc

SRCDIR=	lib/Target/X86/MCTargetDesc
SRCS=	X86AsmBackend.cpp \
	X86ELFObjectWriter.cpp \
	X86MachObjectWriter.cpp \
	X86MCAsmInfo.cpp \
	X86MCCodeEmitter.cpp \
	X86MCTargetDesc.cpp \
	X86WinCOFFObjectWriter.cpp
CFLAGS+= -I${LLVM_SRCS}/${SRCDIR}/..

TGHDRS=	X86GenInstrInfo \
	X86GenRegisterInfo \
	X86GenSubtargetInfo

.include "../clang.lib.mk"
