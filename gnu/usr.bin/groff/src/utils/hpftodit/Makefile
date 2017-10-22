# $FreeBSD: release/10.0.0/gnu/usr.bin/groff/src/utils/hpftodit/Makefile 151504 2005-10-20 10:57:53Z ru $

PROG_CXX=	hpftodit
SRCS=		hpftodit.cpp hpuni.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
