# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/lpr/chkprintcap/Makefile 321275 2017-07-20 01:07:02Z ngie $

.PATH: ${.CURDIR:H}/common_source

PROG=	chkprintcap
MAN=	chkprintcap.8
SRCS=	chkprintcap.c skimprintcap.c

CFLAGS+= -I${.CURDIR:H}/common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
