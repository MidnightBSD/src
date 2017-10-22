#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/10.0.0/usr.bin/fstat/Makefile 235642 2012-05-19 05:07:03Z marcel $

PROG=	fstat
SRCS=	fstat.c fuser.c main.c
LINKS=	${BINDIR}/fstat ${BINDIR}/fuser
DPADD=	${LIBKVM} ${LIBUTIL} ${LIBPROCSTAT}
LDADD=	-lkvm -lutil -lprocstat

MAN1=	fuser.1 fstat.1

.include <bsd.prog.mk>
