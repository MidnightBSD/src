# $FreeBSD: stable/11/usr.bin/clang/llvm-objdump/Makefile 346296 2019-04-16 20:05:24Z dim $

PROG_CXX=	llvm-objdump

SRCDIR=		tools/llvm-objdump
SRCS+=		COFFDump.cpp
SRCS+=		ELFDump.cpp
SRCS+=		MachODump.cpp
SRCS+=		WasmDump.cpp
SRCS+=		llvm-objdump.cpp

LIBADD+=	z

.include "../llvm.prog.mk"
