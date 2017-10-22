#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/9/usr.bin/fstat/Makefile 221807 2011-05-12 10:11:39Z stas $

PROG=	fstat
SRCS=	fstat.c fuser.c main.c
LINKS=	${BINDIR}/fstat ${BINDIR}/fuser
DPADD=	${LIBKVM}
LDADD=	-lkvm -lutil -lprocstat

MAN1=	fuser.1 fstat.1

.include <bsd.prog.mk>
