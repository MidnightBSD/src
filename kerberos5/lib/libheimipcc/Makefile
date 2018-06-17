# $MidnightBSD$
#$FreeBSD: stable/10/kerberos5/lib/libheimipcc/Makefile 291815 2015-12-04 18:59:20Z bdrewery $

LIB=	heimipcc
PRIVATELIB=
LDADD=	-lheimbase -lroken -lpthread
DPADD=	${LIBHEIMBASE} ${LIBROKEN} ${LIBPTHREAD}

SRCS=	\
	client.c \
	common.c

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/include

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
