# $MidnightBSD$
#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.sbin/lpr/lpd/Makefile 321275 2017-07-20 01:07:02Z ngie $

PROG=	lpd
MAN=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c

CFLAGS+= -I${.CURDIR:H}/common_source

WARNS?=	1

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
