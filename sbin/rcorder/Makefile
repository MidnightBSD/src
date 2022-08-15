#       $NetBSD: Makefile,v 1.1 1999/11/23 05:28:20 mrg Exp $
# $FreeBSD: stable/11/sbin/rcorder/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=   rcorder
SRCS=   ealloc.c hash.c rcorder.c
MAN=	rcorder.8

LIBADD=	util

CFLAGS+= -DORDER

#CFLAGS+= -DDEBUG

.include <bsd.prog.mk>
