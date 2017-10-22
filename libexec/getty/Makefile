#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/libexec/getty/Makefile 90164 2002-02-04 02:33:51Z kris $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	gettytab.5 ttys.5 getty.8
WFORMAT=0

.include <bsd.prog.mk>

