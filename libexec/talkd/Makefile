#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/talkd/Makefile,v 1.9 2002/02/04 02:33:51 kris Exp $
# $MidnightBSD: src/libexec/talkd/Makefile,v 1.2 2012/04/11 00:58:36 laffer1 Exp $

PROG=	ntalkd
SRCS=	talkd.c announce.c process.c table.c print.c ttymsg.c
.PATH:  ${.CURDIR}/../../usr.bin/wall
MAN=	talkd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/wall

.include <bsd.prog.mk>
