#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.sbin/lpr/lpc/Makefile 321276 2017-07-20 01:07:15Z ngie $

.PATH: ${.CURDIR:H}/common_source

PROG=	lpc
MAN=	lpc.8
SRCS=	lpc.c cmds.c cmdtab.c movejobs.c
BINGRP=	daemon
BINMODE= 2555

CFLAGS+= -I${.CURDIR:H}/common_source

WARNS?=	0

LIBADD=	lpr edit

.include <bsd.prog.mk>
