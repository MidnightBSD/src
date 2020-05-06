#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/11/libexec/getty/Makefile 275077 2014-11-25 21:18:18Z bapt $

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
LIBADD=	util
MAN=	gettytab.5 ttys.5 getty.8

WARNS?=	1
WFORMAT=0

.include <bsd.prog.mk>

