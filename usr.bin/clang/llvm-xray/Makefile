# $FreeBSD: stable/11/usr.bin/clang/llvm-xray/Makefile 344213 2019-02-16 15:57:29Z dim $

PROG_CXX=	llvm-xray
MAN=

SRCDIR=		tools/llvm-xray
SRCS+=		func-id-helper.cpp
SRCS+=		llvm-xray.cpp
SRCS+=		xray-account.cpp
SRCS+=		xray-color-helper.cpp
SRCS+=		xray-converter.cpp
SRCS+=		xray-extract.cpp
SRCS+=		xray-graph-diff.cpp
SRCS+=		xray-graph.cpp
SRCS+=		xray-registry.cpp
SRCS+=		xray-stacks.cpp

LIBADD+=	z

.include "../llvm.prog.mk"
