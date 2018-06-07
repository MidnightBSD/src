#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.sbin/lpr/pac/Makefile 321275 2017-07-20 01:07:02Z ngie $
# $MidnightBSD$

.PATH: ${.CURDIR:H}/common_source

PROG=	pac
MAN=	pac.8

CFLAGS+= -I${.CURDIR:H}/common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
