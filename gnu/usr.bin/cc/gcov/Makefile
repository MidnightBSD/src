# $FreeBSD: release/10.0.0/gnu/usr.bin/cc/gcov/Makefile 245539 2013-01-17 05:56:28Z andrew $

.include <bsd.own.mk>

.include "../Makefile.inc"
.include "../Makefile.ver"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c errors.c
MAN=	gcov.1

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
