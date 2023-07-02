
.PATH: ${.CURDIR}/../../contrib/cflow/cgraph

PROG=	cgraph
SRCS=	clexer.c cgraph.c

.PATH: .PATH: ${.CURDIR}/../../contrib/cflow/common
SRCS+=	graph.c printgraph.c
MAN=	cgraph.1
CFLAGS+=-I${.CURDIR}/../../contrib/cflow/common 

.include <bsd.prog.mk>
