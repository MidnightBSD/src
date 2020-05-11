# $FreeBSD: stable/11/usr.sbin/bhyveload/Makefile 329145 2018-02-12 01:08:44Z kevans $

PROG=	bhyveload
SRCS=	bhyveload.c
MAN=	bhyveload.8
PACKAGE=	bhyve

LIBADD=	vmmapi

WARNS?=	3

CFLAGS+=-I${SRCTOP}/stand/userboot

.include <bsd.prog.mk>
