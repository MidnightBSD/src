# $FreeBSD: release/10.0.0/gnu/usr.bin/cc/c++filt/Makefile 245539 2013-01-17 05:56:28Z andrew $

NO_MAN=
.include <bsd.own.mk>

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCLIB}/libiberty

PROG=	c++filt
SRCS=	cp-demangle.c

CFLAGS+= -DSTANDALONE_DEMANGLER -DVERSION=\"$(GCC_VERSION)\"

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
