# $FreeBSD: release/10.0.0/lib/clang/clang.lib.mk 239614 2012-08-23 17:08:07Z dim $

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "clang.build.mk"

INTERNALLIB=

.include <bsd.lib.mk>
