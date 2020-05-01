# $FreeBSD: stable/11/usr.sbin/kbdcontrol/Makefile 352280 2019-09-13 07:21:06Z bapt $

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

.include <bsd.prog.mk>
