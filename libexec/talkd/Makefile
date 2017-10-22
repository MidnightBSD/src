#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/libexec/talkd/Makefile 202210 2010-01-13 18:28:58Z ed $

PROG=	ntalkd
SRCS=	talkd.c announce.c process.c table.c print.c ttymsg.c
.PATH:  ${.CURDIR}/../../usr.bin/wall
MAN=	talkd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/wall

.include <bsd.prog.mk>
