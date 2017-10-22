# $FreeBSD: stable/9/usr.bin/clang/clang.prog.mk 239711 2012-08-26 10:30:01Z dim $

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "../../lib/clang/clang.build.mk"

.for lib in ${LIBDEPS}
DPADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

BINDIR?= /usr/bin

.include <bsd.prog.mk>
