# $FreeBSD: stable/11/usr.sbin/ftp-proxy/Makefile 319119 2017-05-29 10:15:41Z ngie $

.PATH:	${SRCTOP}/contrib/pf/ftp-proxy

PROG=	ftp-proxy
MAN=	ftp-proxy.8

SRCS=	ftp-proxy.c filter.c

CFLAGS+=-I${SRCTOP}/contrib/pf/libevent

LIBADD=	event

WARNS?=	3

.include <bsd.prog.mk>
