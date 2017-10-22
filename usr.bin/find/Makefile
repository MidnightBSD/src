#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/7.0.0/usr.bin/find/Makefile 140810 2005-01-25 14:07:25Z ssouhlal $

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
WARNS?=6
CFLAGS+= -DHAVE_SYS_TIMEB_H -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>
