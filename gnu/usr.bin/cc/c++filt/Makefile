# $FreeBSD: stable/9/gnu/usr.bin/cc/c++filt/Makefile 169718 2007-05-19 04:25:59Z kan $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCLIB}/libiberty

PROG=	c++filt
SRCS=	cp-demangle.c
NO_MAN=

CFLAGS+= -DSTANDALONE_DEMANGLER -DVERSION=\"$(GCC_VERSION)\"

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
