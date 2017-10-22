# $FreeBSD: release/10.0.0/usr.sbin/ftp-proxy/ftp-proxy/Makefile 240494 2012-09-14 11:51:49Z glebius $

.PATH:	${.CURDIR}/../../../contrib/pf/ftp-proxy

PROG=	ftp-proxy
MAN=	ftp-proxy.8

SRCS=	ftp-proxy.c filter.c

CFLAGS+= -I${.CURDIR}/../../../contrib/pf/libevent

LDADD+= ${LIBEVENT}
DPADD+= ${LIBEVENT}

WARNS?=	3

.include <bsd.prog.mk>
