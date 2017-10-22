#$FreeBSD: release/10.0.0/kerberos5/lib/libheimipcc/Makefile 255455 2013-09-10 18:40:43Z des $

LIB=	heimipcc
PRIVATELIB=
LDADD=	-lheimbase -lroken -lpthread
DPADD=	${LIBHEIMBASE} ${LIBROKEN} ${LIBPTHREAD}

SRCS=	\
	client.c \
	common.c

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
