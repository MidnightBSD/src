# $FreeBSD: release/10.0.0/usr.bin/clang/clang.prog.mk 239614 2012-08-23 17:08:07Z dim $

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "../../lib/clang/clang.build.mk"

.for lib in ${LIBDEPS}
DPADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

BINDIR?= /usr/bin

.include <bsd.prog.mk>
