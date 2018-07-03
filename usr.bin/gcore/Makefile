# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/gcore/Makefile 283909 2015-06-02 13:07:22Z jhb $

PROG=	gcore
SRCS=	elfcore.c gcore.c
DPADD=	${LIBSBUF} ${LIBUTIL}
LDADD=	-lsbuf -lutil

.if ${MACHINE_ARCH} == "amd64"
SRCS+=	elf32core.c
.endif

WARNS?=	1

.include <bsd.prog.mk>
