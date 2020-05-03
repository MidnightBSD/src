# $FreeBSD: stable/11/lib/clang/Makefile 310618 2016-12-26 20:36:37Z dim $

.include <src.opts.mk>

# These have to be built in order.
SUBDIR=		libllvm
SUBDIR+=	libclang
.if ${MK_LLDB} != "no"
SUBDIR+=	liblldb
.endif
SUBDIR+=	headers

.include <bsd.subdir.mk>
