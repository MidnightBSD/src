#$FreeBSD: stable/11/kerberos5/lib/libheimipcs/Makefile 275022 2014-11-25 09:57:42Z bapt $

LIB=	heimipcs
PRIVATELIB=
LIBADD=	heimbase roken pthread

SRCS=	\
	server.c \
	common.c

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
