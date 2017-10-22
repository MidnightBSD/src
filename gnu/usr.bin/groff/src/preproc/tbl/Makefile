# $FreeBSD: release/10.0.0/gnu/usr.bin/groff/src/preproc/tbl/Makefile 114412 2003-05-01 13:22:21Z ru $

PROG_CXX=	tbl
SRCS=		main.cpp table.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
