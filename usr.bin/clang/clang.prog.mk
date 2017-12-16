# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/clang/clang.prog.mk 263508 2014-03-21 17:53:59Z dim $

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "../../lib/clang/clang.build.mk"

.for lib in ${LIBDEPS}
DPADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

DPADD+=	${LIBNCURSES}
LDADD+=	-lncurses

BINDIR?= /usr/bin

.include <bsd.prog.mk>
