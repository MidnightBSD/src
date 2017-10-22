# $FreeBSD: release/10.0.0/lib/clang/lldb.lib.mk 255722 2013-09-20 01:52:02Z emaste $

LLDB_SRCS= ${.CURDIR}/../../../contrib/llvm/tools/lldb

CFLAGS+=-I${LLDB_SRCS}/include -I${LLDB_SRCS}/source
CXXFLAGS+=-std=c++11 -DLLDB_DISABLE_PYTHON                      

.include "clang.lib.mk"
