# $FreeBSD: release/9.2.0/lib/clang/clang.lib.mk 239711 2012-08-26 10:30:01Z dim $

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "clang.build.mk"

INTERNALLIB=

.include <bsd.lib.mk>
