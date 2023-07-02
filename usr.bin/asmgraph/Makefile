
.PATH: ${.CURDIR}/../../contrib/cflow/asmgraph

PROG=	asmgraph
SRCS=	aslexer.c nasmlexer.c asmgraph.c

.PATH: ${.CURDIR}/../../contrib/cflow/common
SRCS+=	graph.c printgraph.c

MAN=	asmgraph.1
CFLAGS+=-I${.CURDIR}/../../contrib/cflow/common

.include <bsd.prog.mk>
