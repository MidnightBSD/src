#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/find/Makefile,v 1.18 2005/01/25 14:07:25 ssouhlal Exp $
# $MidnightBSD: src/usr.bin/find/Makefile,v 1.2 2007/09/11 23:01:20 laffer1 Exp $

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
CFLAGS+= -DHAVE_SYS_TIMEB_H -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>
