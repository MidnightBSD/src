#	$MidnightBSD: src/usr.sbin/rdate/Makefile,v 1.1 2006/08/22 05:52:12 laffer1 Exp $ 
#	$OpenBSD: Makefile,v 1.7 2003/11/20 23:23:09 avsm Exp $

PROG=	rdate
SRCS=	rdate.c rfc868time.c ntp.c ntpleaps.c
CFLAGS+=-Wall -ansi -pedantic -std=c99 -DNO_UTIL

MAN=	rdate.8

.include <bsd.prog.mk>
