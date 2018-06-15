# $MidnightBSD$
# $FreeBSD: stable/10/gnu/usr.bin/cc/c++filt/Makefile 276486 2014-12-31 23:25:37Z ngie $

MAN=
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
