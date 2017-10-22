#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/libexec/rlogind/Makefile 160742 2006-07-27 12:05:05Z yar $

.include <bsd.own.mk>

PROG=	rlogind
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	2

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
