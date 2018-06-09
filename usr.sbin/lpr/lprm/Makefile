# $MidnightBSD$
#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.sbin/lpr/lprm/Makefile 321275 2017-07-20 01:07:02Z ngie $

.PATH: ${.CURDIR:H}/common_source

BINDIR=	/usr/bin

PROG=	lprm
BINOWN=	root
BINGRP=	daemon
BINMODE= 6555

CFLAGS+= -I${.CURDIR:H}/common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
