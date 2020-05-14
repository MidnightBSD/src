# $FreeBSD: stable/11/usr.sbin/apmd/Makefile 352280 2019-09-13 07:21:06Z bapt $

PROG=	apmd
MAN=	apmd.8
MANSUBDIR= /i386
SRCS=	apmd.c apmdlex.l apmdparse.y y.tab.h

PACKAGE=apm

WARNS?=	3

CFLAGS+= -I${.CURDIR}

test:
	./apmd -d -f etc/apmd.conf -n

.include <bsd.prog.mk>
