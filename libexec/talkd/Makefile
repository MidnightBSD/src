#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/libexec/talkd/Makefile 90164 2002-02-04 02:33:51Z kris $

PROG=	ntalkd
SRCS=	talkd.c announce.c process.c table.c print.c ttymsg.c
.PATH:  ${.CURDIR}/../../usr.bin/wall
MAN=	talkd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/wall
WFORMAT=0

.include <bsd.prog.mk>
