#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: release/7.0.0/games/fortune/fortune/Makefile 142022 2005-02-17 18:06:37Z ru $

PROG=	fortune
MAN=	fortune.6
WARNS?=	2
CFLAGS+=-DDEBUG -I${.CURDIR}/../strfile

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -m brass
