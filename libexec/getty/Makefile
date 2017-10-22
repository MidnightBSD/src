#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/libexec/getty/Makefile 201380 2010-01-02 09:50:19Z ed $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	gettytab.5 ttys.5 getty.8

WARNS?=	1
WFORMAT=0

.include <bsd.prog.mk>

