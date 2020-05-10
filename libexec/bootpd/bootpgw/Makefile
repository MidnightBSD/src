# Makefile
# $FreeBSD: stable/11/libexec/bootpd/bootpgw/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=	bootpgw
MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
