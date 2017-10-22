# $FreeBSD: release/10.0.0/libexec/rpc.rquotad/Makefile 207736 2010-05-07 00:41:12Z mckusick $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
