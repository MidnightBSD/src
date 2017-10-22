# $FreeBSD: release/10.0.0/tools/regression/kgssapi/Makefile 204311 2010-02-25 14:42:26Z ru $

PROG=	gsstest
NO_MAN=
WARNS?=	2
DPADD=	${LIBGSSAPI} ${LIBGSSAPI_KRB5}
LDADD=	-lgssapi -lgssapi_krb5
DEBUG_FLAGS= -g -O0

.include <bsd.prog.mk>
