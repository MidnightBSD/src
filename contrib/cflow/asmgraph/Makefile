# $FreeBSD$

PROG=	asmgraph
SRCS=	aslexer.c nasmlexer.c asmgraph.c graph.c printgraph.c
CLEANFILES=	*~ *.core
MAN=	asmgraph.1
CFLAGS+=-I${.CURDIR}/../common #-pedantic -g -O0

.PATH: ${.CURDIR}/../common

.include <bsd.prog.mk>
