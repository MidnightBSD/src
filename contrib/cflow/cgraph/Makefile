# $FreeBSD$

PROG=	cgraph
SRCS=	clexer.c cgraph.c graph.c printgraph.c
CLEANFILES=	*~ *.core
MAN=	cgraph.1
CFLAGS+=-I${.CURDIR}/../common #-pedantic -g -O0

.PATH: ${.CURDIR}/../common

.include <bsd.prog.mk>
