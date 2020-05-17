# $FreeBSD: stable/11/usr.bin/colldef/Makefile 291127 2015-11-21 10:52:32Z bapt $

PROG=	colldef
SRCS=	parse.y scan.l y.tab.h
LFLAGS=	-8 -i
CFLAGS+=-I. -I${.CURDIR}
CFLAGS+=-DCOLLATE_DEBUG -DYY_NO_UNPUT -DYY_NO_INPUT
LIBADD=	l

WARNS?=	2

.include <bsd.prog.mk>
