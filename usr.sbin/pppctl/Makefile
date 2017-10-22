# $FreeBSD: release/10.0.0/usr.sbin/pppctl/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	pppctl
MAN=	pppctl.8

WARNS?=	2

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
