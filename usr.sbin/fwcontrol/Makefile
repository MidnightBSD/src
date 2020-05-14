# $FreeBSD: stable/11/usr.sbin/fwcontrol/Makefile 319119 2017-05-29 10:15:41Z ngie $

PROG=	fwcontrol
SRCS=	fwcontrol.c fwcrom.c fwdv.c fwmpegts.c
MAN=	fwcontrol.8
WARNS?=	3

.PATH:	${SRCTOP}/sys/dev/firewire

SDIR= ${SRCTOP}/sys
CFLAGS+=-I${.CURDIR} -I${SDIR}

.include <bsd.prog.mk>
