# $FreeBSD: release/10.0.0/tools/regression/rpcsec_gss/Makefile 204311 2010-02-25 14:42:26Z ru $

PROG=	rpctest
NO_MAN=
WARNS?=	6
DPADD=	${LIBRPCSEC_GSS}
LDADD=	-lrpcsec_gss
DEBUG_FLAGS= -g -O0

.include <bsd.prog.mk>
