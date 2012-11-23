#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $MidnightBSD$

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm

WARNS?=	3

.include <bsd.prog.mk>
