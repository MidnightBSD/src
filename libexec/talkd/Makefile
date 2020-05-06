#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/11/libexec/talkd/Makefile 321269 2017-07-20 00:58:19Z ngie $

PROG=	ntalkd
SRCS=	talkd.c announce.c process.c table.c print.c ttymsg.c
.PATH:  ${SRCTOP}/usr.bin/wall
MAN=	talkd.8
CFLAGS+=-I${SRCTOP}/usr.bin/wall

.include <bsd.prog.mk>
