# $MidnightBSD: src/usr.bin/sdiff/Makefile,v 1.1 2007/06/24 04:53:23 laffer1 Exp $
#	$OpenBSD: Makefile,v 1.4 2006/02/20 08:38:18 otto Exp $

PROG=sdiff
SRCS=common.c edit.c sdiff.c
COPTS+=-Wall -W

WARNS?=	0

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
