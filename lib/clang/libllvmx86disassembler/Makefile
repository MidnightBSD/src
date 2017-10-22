# $FreeBSD: stable/9/lib/clang/libllvmx86disassembler/Makefile 231919 2012-02-19 23:35:56Z dim $

.include <bsd.own.mk>

LIB=	llvmx86disassembler

SRCDIR=	lib/Target/X86/Disassembler
INCDIR=	lib/Target/X86
SRCS=	X86Disassembler.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	X86DisassemblerDecoder.c
.endif

TGHDRS=	X86GenDisassemblerTables \
	X86GenEDInfo \
	X86GenInstrInfo \
	X86GenRegisterInfo

.include "../clang.lib.mk"
