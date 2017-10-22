# $FreeBSD: release/10.0.0/gnu/usr.bin/groff/src/utils/lookbib/Makefile 114412 2003-05-01 13:22:21Z ru $

PROG_CXX=	lookbib
SRCS=		lookbib.cpp
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
