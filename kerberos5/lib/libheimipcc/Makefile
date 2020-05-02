#$FreeBSD: stable/11/kerberos5/lib/libheimipcc/Makefile 284289 2015-06-11 21:13:05Z sjg $

LIB=	heimipcc
PRIVATELIB=
LIBADD=	heimbase roken pthread

SRCS=	\
	client.c \
	common.c

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/include

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
