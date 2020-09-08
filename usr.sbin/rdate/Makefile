#	$MidnightBSD: src/usr.sbin/rdate/Makefile,v 1.2 2013/01/03 02:54:52 laffer1 Exp $ 

PROG=	rdate
SRCS=	rdate.c rfc868time.c ntp.c ntpleaps.c
CFLAGS+=-Wall -ansi -pedantic -std=c99 -DNO_UTIL

MAN=	rdate.8

WARNS=	0

.include <bsd.prog.mk>
